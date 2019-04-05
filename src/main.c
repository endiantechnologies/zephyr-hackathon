#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr.h>
#include <assert.h>

#include "ble_manager.h"
#include "vcc.h"

void main(void) {
	int ret;

	LOG_INF("Initializing BLE manager");
	ret = ble_init();
	if (ret != 0){
		LOG_ERR("Could not initialize BLE manager");
	}

	while (1) {
		k_sleep(K_SECONDS(5));

		struct vcc_sample sample;
		ret = vcc_get_latest(&sample);
		if (ret != 0) {
			LOG_ERR("vcc_get_latest returned %d", ret);
		} else {
			LOG_DBG("vcc = %hi, timestamp = %u",
				sample.sample, sample.timestamp);
		}
	}
}
