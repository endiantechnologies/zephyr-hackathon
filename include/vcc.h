#include <zephyr.h>

struct vcc_sample {
	s16_t sample;
	u64_t timestamp;
} __packed;

int vcc_get_latest(struct vcc_sample *sample);
