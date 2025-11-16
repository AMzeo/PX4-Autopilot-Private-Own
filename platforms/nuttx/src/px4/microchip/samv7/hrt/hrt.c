/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file hrt.c
 *
 * High-resolution timer for SAMV7 using TC0 (Timer/Counter 0)
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>

#include <assert.h>
#include <debug.h>
#include <time.h>
#include <queue.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <board_config.h>
#include <drivers/drv_hrt.h>

#include "arm_internal.h"
#include "hardware/sam_pmc.h"
#include "hardware/sam_tc.h"

#ifdef CONFIG_DEBUG_HRT
#  define hrtinfo _info
#else
#  define hrtinfo(x...)
#endif

#ifdef HRT_TIMER

/* HRT configuration for SAMV7 TC0 */
#if HRT_TIMER == 0
# define HRT_TIMER_BASE		SAM_TC012_BASE
# define HRT_TIMER_CHANNEL	0
# define HRT_TIMER_VECTOR	SAM_IRQ_TC0
# define HRT_TIMER_CLOCK	BOARD_MCK_FREQUENCY
# define HRT_TIMER_PCER	(1 << SAM_PID_TC0)
#else
# error HRT_TIMER must be 0 for SAMV7 (TC0 Channel 0)
#endif

/* Minimum/maximum deadlines */
#define HRT_INTERVAL_MIN	50
#define HRT_INTERVAL_MAX	50000

/* Actual timer frequency - MCK/32 prescaler (TC_CMR_TCCLKS_MCK32) */
#define HRT_ACTUAL_FREQ		(HRT_TIMER_CLOCK / 32)

/* Timer register addresses for TC0 Channel 0 */
#define rCCR	(HRT_TIMER_BASE + SAM_TC_CCR_OFFSET)
#define rCMR	(HRT_TIMER_BASE + SAM_TC_CMR_OFFSET)
#define rCV	(HRT_TIMER_BASE + SAM_TC_CV_OFFSET)
#define rRA	(HRT_TIMER_BASE + SAM_TC_RA_OFFSET)
#define rRC	(HRT_TIMER_BASE + SAM_TC_RC_OFFSET)
#define rSR	(HRT_TIMER_BASE + SAM_TC_SR_OFFSET)
#define rIER	(HRT_TIMER_BASE + SAM_TC_IER_OFFSET)
#define rIDR	(HRT_TIMER_BASE + SAM_TC_IDR_OFFSET)
#define rIMR	(HRT_TIMER_BASE + SAM_TC_IMR_OFFSET)

/* TC Channel Control Register bits */
#define TC_CCR_CLKEN		(1 << 0)
#define TC_CCR_CLKDIS		(1 << 1)
#define TC_CCR_SWTRG		(1 << 2)

/* TC register definitions - use NuttX hardware definitions */
/* NuttX provides these in hardware/sam_tc.h */

/* Forward declarations */
static int hrt_tim_isr(int irq, void *context, void *arg);

/* Callout list */
static struct sq_queue_s callout_queue;

/* Latency histogram */
#define LATENCY_BUCKET_COUNT 8
__EXPORT const uint16_t latency_buckets[LATENCY_BUCKET_COUNT] = { 1, 2, 5, 10, 20, 50, 100, 1000 };
__EXPORT uint32_t latency_actual_min = UINT32_MAX;
__EXPORT uint32_t latency_actual_max = 0;

/* HRT clock counter */
static uint64_t hrt_absolute_time_base;
static uint32_t hrt_counter_wrap_count;

/**
 * Get absolute time
 */
hrt_abstime hrt_absolute_time(void)
{
	uint64_t base;
	uint32_t count;
	irqstate_t flags;

	flags = enter_critical_section();

	/* Atomic read of base and counter */
	base = hrt_absolute_time_base;
	count = getreg32(rCV);

	leave_critical_section(flags);

	/* Convert ticks to microseconds */
	uint64_t total_ticks = base + count;
	return (total_ticks * 1000000ULL) / HRT_ACTUAL_FREQ;
}

/**
 * Initialize the HRT
 */
void hrt_init(void)
{
	syslog(LOG_ERR, "[hrt] hrt_init starting\n");
	sq_init(&callout_queue);

	/* Enable peripheral clock for TC0 */
	uint32_t regval = getreg32(SAM_PMC_PCER0);
	syslog(LOG_ERR, "[hrt] SAM_PMC_PCER0 before: 0x%08lx\n", (unsigned long)regval);
	regval |= HRT_TIMER_PCER;
	putreg32(regval, SAM_PMC_PCER0);
	syslog(LOG_ERR, "[hrt] SAM_PMC_PCER0 after: 0x%08lx\n", (unsigned long)getreg32(SAM_PMC_PCER0));

	/* Disable TC clock */
	putreg32(TC_CCR_CLKDIS, rCCR);

	/* Configure TC channel mode:
	 * - Waveform mode
	 * - Up mode with automatic reset on RC compare
	 * - Use MCK/8 prescaler (for ~150MHz MCK gives ~19MHz)
	 */
	uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP;

	/* Select prescaler to get as close to 1MHz as possible */
	if (HRT_TIMER_CLOCK / 8 > 1000000) {
		cmr |= TC_CMR_TCCLKS_MCK32;  /* MCK/32 */
		syslog(LOG_ERR, "[hrt] Using MCK/32 prescaler\n");
	} else {
		cmr |= TC_CMR_TCCLKS_MCK8;   /* MCK/8 */
		syslog(LOG_ERR, "[hrt] Using MCK/8 prescaler\n");
	}

	putreg32(cmr, rCMR);

	/* Set RC to maximum value for free-running mode */
	putreg32(0xFFFFFFFF, rRC);

	/* Disable all interrupts */
	putreg32(0xFFFFFFFF, rIDR);

	/* Clear status */
	(void)getreg32(rSR);

	/* Enable TC clock and trigger */
	putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

	/* Initialize absolute time base */
	hrt_absolute_time_base = 0;
	hrt_counter_wrap_count = 0;

	/* Attach interrupt handler */
	irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

	/* Enable overflow interrupt (RC compare for wraparound) */
	putreg32(TC_INT_CPCS, rIER);

	/* Enable interrupt at NVIC level */
	up_enable_irq(HRT_TIMER_VECTOR);

	syslog(LOG_ERR, "[hrt] HRT initialized with interrupts, testing...\n");

	/* Test that timer is running */
	uint32_t cv1 = getreg32(rCV);
	for (volatile int i = 0; i < 100000; i++);
	uint32_t cv2 = getreg32(rCV);
	syslog(LOG_ERR, "[hrt] Counter test: CV1=0x%08lx CV2=0x%08lx diff=%lu\n",
		(unsigned long)cv1, (unsigned long)cv2, (unsigned long)(cv2 - cv1));

	hrtinfo("HRT initialized\n");
}

/**
 * Call callout entries
 */
static void hrt_call_invoke(void)
{
	struct hrt_call *call;
	hrt_abstime deadline __attribute__((unused));

	/* Read time once at start to avoid critical section issues */
	uint32_t now_cv = getreg32(rCV);
	uint64_t base = hrt_absolute_time_base;
	uint64_t now_ticks = base + now_cv;
	hrt_abstime now = (now_ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

	int max_iterations = 16;  /* Prevent infinite loops */

	while (max_iterations-- > 0) {
		call = (struct hrt_call *)sq_peek(&callout_queue);

		if (call == NULL) {
			break;
		}

		if (call->deadline > now) {
			break;
		}

		sq_rem(&call->link, &callout_queue);
		call->deadline = 0;

		/* Invoke callback */
		if (call->callout) {
			call->callout(call->arg);
		}
	}
}

/**
 * Reschedule next alarm
 * NOTE: Called from within critical section, so don't call hrt_absolute_time()
 */
static void hrt_call_reschedule(void)
{
	/* Read counter directly - already in critical section from caller */
	uint32_t now_cv = getreg32(rCV);
	uint64_t base = hrt_absolute_time_base;
	uint64_t now_ticks = base + now_cv;
	hrt_abstime now_usec = (now_ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

	struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

	if (next != NULL) {
		hrt_abstime deadline = next->deadline;

		if (deadline < now_usec) {
			deadline = now_usec + HRT_INTERVAL_MIN;
		}
	}
}

/**
 * HRT interrupt handler
 */
static int hrt_tim_isr(int irq, void *context, void *arg)
{
	uint32_t status;

	/* Read and clear status */
	status = getreg32(rSR);

	/* Handle counter overflow/wrap */
	if (status & TC_INT_CPCS) {
		hrt_counter_wrap_count++;
	}

	/* Process callouts */
	hrt_call_invoke();

	/* Reschedule next interrupt */
	hrt_call_reschedule();

	return OK;
}

/**
 * Call callout function at specified time
 */
void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime, hrt_callout callout, void *arg)
{
	if (entry == NULL || callout == NULL) {
		return;
	}

	irqstate_t flags = enter_critical_section();

	/* Remove from queue if already scheduled */
	sq_rem(&entry->link, &callout_queue);

	entry->deadline = calltime;
	entry->callout = callout;
	entry->arg = arg;

	/* Insert into queue in deadline order */
	struct hrt_call *call;
	struct hrt_call *prev = NULL;

	for (call = (struct hrt_call *)sq_peek(&callout_queue); call != NULL;
	     call = (struct hrt_call *)sq_next(&call->link)) {
		if (call->deadline > calltime) {
			break;
		}

		prev = call;
	}

	if (prev == NULL) {
		sq_addfirst(&entry->link, &callout_queue);

	} else {
		sq_addafter(&prev->link, &entry->link, &callout_queue);
	}

	hrt_call_reschedule();

	leave_critical_section(flags);
}

/**
 * Call callout function after delay
 */
void hrt_call_after(struct hrt_call *entry, hrt_abstime delay, hrt_callout callout, void *arg)
{
	hrt_call_at(entry, hrt_absolute_time() + delay, callout, arg);
}

/**
 * Call callout function at periodic intervals
 */
void hrt_call_every(struct hrt_call *entry, hrt_abstime delay, hrt_abstime interval, hrt_callout callout, void *arg)
{
	entry->period = interval;
	hrt_call_after(entry, delay, callout, arg);
}

/**
 * Cancel a callout
 */
void hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = enter_critical_section();

	sq_rem(&entry->link, &callout_queue);
	entry->deadline = 0;
	entry->period = 0;

	leave_critical_section(flags);
}

/* CPU load monitoring support function */
void hrt_store_absolute_time(volatile hrt_abstime *t)
{
	*t = hrt_absolute_time();
}

#endif /* HRT_TIMER */
