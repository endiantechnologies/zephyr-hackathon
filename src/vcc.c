#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(vcc);

#include <zephyr.h>
#include <init.h>
#include <adc.h>

#include "vcc.h"

/* Channel number 8 <=> AIN7 <=> P0.31 */
/* Channel number 4 <=> AIN3 <=> P0.05 */
#define VCC_ADC_CHANNEL 4
#define VCC_POLLER_STACK_SIZE 512

/* Normally, the ADC uses VCC as its reference. But it doesn't make much sense
 * if VCC is what we're trying to measure. Luckily, the nRF52 supplies an
 * internal, constant 0.6V reference voltage. */
#define VCC_ADC_REF ADC_REF_INTERNAL
#define VCC_POLL_RESOLUTION 12		/* 12 bit resolution */
#define VCC_POLL_OVERSAMP 3		/* Take average over 2^3 samples */
#define VCC_POLL_SLEEP_TIME (1 * 1000)	/* One second sampling interval */

K_THREAD_STACK_DEFINE(vcc_poller_stack_area, VCC_POLLER_STACK_SIZE);
struct k_thread vcc_poller_thread_data;
static k_tid_t vcc_poller_tid;

struct k_poll_signal vcc_signal;
struct k_poll_event  vcc_event;

/* Signal definition for data ready */
#define VCC_DATA_RDY 0x100

static struct vcc_sample cur_sample;

/* The function which is run by the polling thread. Observant readers will note
 * that a thread is really not necessary here -- it would be better to just call
 * adc_read directly from vcc_get_latest(). */
static void vcc_poll_fn(void *dev_ptr, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;
	s16_t sample;
	u64_t timestamp;
	struct device *adc_dev = dev_ptr;

	struct adc_sequence seq = {
		.resolution	  = VCC_POLL_RESOLUTION,
		.oversampling = VCC_POLL_OVERSAMP,
		.channels  = 1 << VCC_ADC_CHANNEL,
		.buffer	 = &sample,
		.buffer_size  = sizeof(sample),
	};

	while (1) {
		ret = adc_read(adc_dev, &seq);
		timestamp = k_uptime_get();
		if (ret != 0) {
			LOG_WRN("adc_read returned %d", ret);
			goto sleep;
		}

		/* LOG_DBG("sample = %hi, timestamp = %u", */
			/* sample, timestamp); */

		/* lock scheduler */
		k_sched_lock();
		cur_sample.sample = sample;
		cur_sample.timestamp = timestamp;
		k_sched_unlock();
	sleep:
		k_sleep(VCC_POLL_SLEEP_TIME);
	}
}

int vcc_get_latest(struct vcc_sample *sample)
{
	/* Note: k_sched_lock() is a convenient, but pretty hand-wavy, way to
	 * synchronize data access. If you're new to Zephyr, you may find it
	 * educational to replace it with something more idiomatic. Check out
	 * include/kernel.h for some nice data structures:
	 *     k_msg, k_mbox, k_pipe, k_queue, k_fifo, k_lifo and k_stack all
	 * have their use cases. See also include/atomic.h */
	k_sched_lock();

	if ((0 == cur_sample.timestamp) && (0 == cur_sample.sample)) {
		/* ADC hasn't been sampled yet */
		return -EAGAIN;
	}

	*sample = cur_sample;

	k_sched_unlock();
	return 0;
}

/* Initialization function */
int vcc_init(struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;
	struct device *adc_dev;
	static struct adc_channel_cfg channel_cfg;

	adc_dev = device_get_binding(DT_ADC_0_NAME);
	__ASSERT_NO_MSG(adc_dev);

	/* Configure the ADC  */
	/* [V(P) – V(N) ] * GAIN/REFERENCE * 2(RESOLUTION - m) */
	// = (1/6) / 0.6 = 0.1
	channel_cfg = (struct adc_channel_cfg) {
		.channel_id	  = VCC_ADC_CHANNEL,
		.gain		  = ADC_GAIN_1_6,
		.reference	  = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.input_positive	  = VCC_ADC_CHANNEL,
		.differential	  = 0,
		.input_negative	  = 0
	};
	ret = adc_channel_setup(adc_dev, &channel_cfg);
	__ASSERT(0 == ret, "adc_channel_setup returned %d", ret);

	k_poll_signal_init(&vcc_signal);
	vcc_event = (struct k_poll_event)
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
					 K_POLL_MODE_NOTIFY_ONLY,
					 &vcc_signal);


	/* Start ADC polling thread */
	vcc_poller_tid = k_thread_create(
		&vcc_poller_thread_data,
		vcc_poller_stack_area,
		VCC_POLLER_STACK_SIZE,
		vcc_poll_fn,		// entry point
		adc_dev,		// *p1 -- sensor port ID
		NULL,			// *p2 -- unused
		NULL,			// *p3 -- unused
		9,
		0,			// flags
		0			// delay
		);

	return 0;
}

SYS_INIT(vcc_init, POST_KERNEL, 80);
