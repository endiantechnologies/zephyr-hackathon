#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ble_manager);

#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <gpio.h>

#include "vcc.h"

static u8_t write_led_buffer[2];
static u8_t adc_gatt_buffer[100];

// Primary service: 0ded4e00-e8e3-2083-094f-431901433ad2

/* This is the service */
static struct bt_uuid_128 example_service_uuid =
	BT_UUID_INIT_128(0xd2, 0x3a, 0x43, 0x01, 0x19, 0x43, 0x4f, 0x09, 0x83,
			 0x20, 0xe3, 0xe8,
			 /* important part: */ 0x00, 0x3e, 0x7d, 0xad);

static const struct bt_uuid_128 adc_read_uuid =
	BT_UUID_INIT_128(0xd2, 0x3a, 0x43, 0x01, 0x19, 0x43, 0x4f, 0x09, 0x83,
			 0x20, 0xe3, 0xe8,
			 /* important part: */ 0x01, 0x3e, 0x7d, 0xad);

static const struct bt_uuid_128 write_led_uuid =
	BT_UUID_INIT_128(0xd2, 0x3a, 0x43, 0x01, 0x19, 0x43, 0x4f, 0x09, 0x83,
			 0x20, 0xe3, 0xe8,
			 /* important part: */ 0x02, 0x3e, 0x7d, 0xad);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0a, 0x18), // FIXME: what's this
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0xd2, 0x3a, 0x43, 0x01, 0x19, 0x43,
		      0x4f, 0x09, 0x83, 0x20, 0xe3, 0xe8,
		      0x00, 0x3e, 0x7d, 0xad),
};

static ssize_t read_adc(struct bt_conn *conn,
			const struct bt_gatt_attr *attr, void *buf,
			u16_t len, u16_t offset)
{
	LOG_DBG("len = %u, offset = %u", len, offset);
	void *value = attr->user_data;
	int ret;
	struct vcc_sample sample;

	ret = vcc_get_latest(&sample);
	if (ret != 0) {
		LOG_ERR("read_adc returned %d", ret);
		return ret;
	}
	memcpy(value, &sample, sizeof(sample));

	return bt_gatt_attr_read(
		conn, attr, buf, len, offset, value, sizeof(sample));
}


/* Hardcoding the number of LEDs is clearly not the most portable solution. But
 * it is the fastest to implement :) */
#define NUM_LEDS 4
static struct device *gpio_dev[NUM_LEDS];
static int gpio_pin[NUM_LEDS];

static int do_write_led(u8_t led, u8_t val)
{
	LOG_DBG("Writing %X to %X", !val, led);

	/* LEDs are enabled by writing 0, so we flip the value */
	return gpio_pin_write(gpio_dev[led], gpio_pin[led], !val);
}

static void init_gpio_leds(void)
{
	gpio_dev[0] = device_get_binding(DT_GPIO_LEDS_LED_0_GPIO_CONTROLLER);
	gpio_dev[1] = device_get_binding(DT_GPIO_LEDS_LED_1_GPIO_CONTROLLER);
	gpio_dev[2] = device_get_binding(DT_GPIO_LEDS_LED_2_GPIO_CONTROLLER);
	gpio_dev[3] = device_get_binding(DT_GPIO_LEDS_LED_3_GPIO_CONTROLLER);

	__ASSERT_NO_MSG(gpio_dev[0]);
	__ASSERT_NO_MSG(gpio_dev[1]);
	__ASSERT_NO_MSG(gpio_dev[2]);
	__ASSERT_NO_MSG(gpio_dev[3]);

	gpio_pin[0] = DT_GPIO_LEDS_LED_0_GPIO_PIN;
	gpio_pin[1] = DT_GPIO_LEDS_LED_1_GPIO_PIN;
	gpio_pin[2] = DT_GPIO_LEDS_LED_2_GPIO_PIN;
	gpio_pin[3] = DT_GPIO_LEDS_LED_3_GPIO_PIN;

	/* Configure GPIOs, turn them off */
	for (u8_t i = 0; i < NUM_LEDS; i++) {
		gpio_pin_configure(gpio_dev[i], gpio_pin[i], GPIO_DIR_OUT);
		do_write_led(i, 0);
	}
}

/* Callback that gets called when someone writes to the led characteristic.
 * Expects user to write byte array containing [LED_IDX, LED_VAL]. */
static ssize_t write_led(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset, u8_t flags)
{
	if (len != sizeof(write_led_buffer)) {
		return -EINVAL;
	}

	// command written is contained in *buf
	u8_t *cmd = (u8_t *)buf;
	u8_t led = cmd[0];
	u8_t val = cmd[1];

	if (led > NUM_LEDS) {
		return -EINVAL;
	}
	if (val > 1) {
		return -EINVAL;
	}

	do_write_led(led, val);

	return len;
}

static struct bt_gatt_attr service_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(&example_service_uuid),
/* -- Read ADC characteristic */
	BT_GATT_CHARACTERISTIC(&adc_read_uuid.uuid,	/* UUID */
			       BT_GATT_CHRC_READ,	/* Properties, i.e. read, write, .. */
			       BT_GATT_PERM_READ,	/* Permissions */
			       read_adc,		/* Read callback */
			       NULL,			/* Write callback */
			       adc_gatt_buffer),	/* Value storage */

/* -- Write LED characteristic */
	BT_GATT_CHARACTERISTIC(&write_led_uuid.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL,
			       write_led,
			       write_led_buffer),
};

static struct bt_gatt_service example_service = BT_GATT_SERVICE(service_attrs);

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
	} else {
		LOG_INF("Connected");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

int ble_init(void)
{
	int err;

	init_gpio_leds();

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	bt_conn_cb_register(&conn_callbacks);
	bt_gatt_service_register(&example_service);

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_INF("Advertising successfully started");
	return 0;
}
