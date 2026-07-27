#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#if __has_include(<zephyr/version.h>)
#include <zephyr/version.h>
#else
#include <version.h>
#endif

#include "nexting_device.h"

#define NEXTING_SERVICE_UUID \
	BT_UUID_128_ENCODE(0x6eadc0de, 0x0001, 0x4a21, 0x9c5e, 0x1b7f3d9e42a0)
#define NEXTING_DOWNLINK_UUID \
	BT_UUID_128_ENCODE(0x6eadc0de, 0x0002, 0x4a21, 0x9c5e, 0x1b7f3d9e42a0)
#define NEXTING_UPLINK_UUID \
	BT_UUID_128_ENCODE(0x6eadc0de, 0x0003, 0x4a21, 0x9c5e, 0x1b7f3d9e42a0)
#define NEXTING_DEVICE_INFO_UUID \
	BT_UUID_128_ENCODE(0x6eadc0de, 0x0004, 0x4a21, 0x9c5e, 0x1b7f3d9e42a0)

#define DEVICE_INFO_JSON                                                        \
	"{\"protocol\":\"nexting-device\",\"spec\":\"0.2.0-experimental.0\","      \
	"\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"" CONFIG_BOARD \
	"\",\"fw\":\"0.2.0\",\"max_message_bytes\":4096,"                      \
	"\"max_summary_bytes\":240,\"display_name\":\"Nexting Reference\","    \
	"\"button_count\":2,\"approval_button_count\":2,"                      \
	"\"custom_button_count\":0}"

#define UPLINK_FRAME_CAPACITY 384U
#define ADVERTISING_RETRY_DELAY K_MSEC(100)

#if ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(4, 3, 0)
#define NEXTING_CONNECTABLE_ADV_OPTION BT_LE_ADV_OPT_CONN
#else
#define NEXTING_CONNECTABLE_ADV_OPTION BT_LE_ADV_OPT_CONNECTABLE
#endif

#define NEXTING_CONNECTABLE_ADV_PARAMS                                        \
	BT_LE_ADV_PARAM(NEXTING_CONNECTABLE_ADV_OPTION,                         \
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#if DT_NODE_EXISTS(DT_ALIAS(nexting_allow))
#define ALLOW_NODE DT_ALIAS(nexting_allow)
#elif DT_NODE_EXISTS(DT_ALIAS(sw0))
#define ALLOW_NODE DT_ALIAS(sw0)
#else
#error "Board needs a nexting-allow or sw0 alias"
#endif

#if DT_NODE_EXISTS(DT_ALIAS(nexting_deny))
#define DENY_NODE DT_ALIAS(nexting_deny)
#elif DT_NODE_EXISTS(DT_ALIAS(sw1))
#define DENY_NODE DT_ALIAS(sw1)
#else
#error "Board needs a nexting-deny or sw1 alias"
#endif

#if DT_NODE_EXISTS(DT_ALIAS(nexting_led))
#define PENDING_LED_NODE DT_ALIAS(nexting_led)
#elif DT_NODE_EXISTS(DT_ALIAS(led0))
#define PENDING_LED_NODE DT_ALIAS(led0)
#else
#error "Board needs a nexting-led or led0 alias"
#endif

static const struct gpio_dt_spec allow_button = GPIO_DT_SPEC_GET(ALLOW_NODE, gpios);
static const struct gpio_dt_spec deny_button = GPIO_DT_SPEC_GET(DENY_NODE, gpios);
static const struct gpio_dt_spec pending_led = GPIO_DT_SPEC_GET(PENDING_LED_NODE, gpios);

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(NEXTING_SERVICE_UUID);
static struct bt_uuid_128 downlink_uuid = BT_UUID_INIT_128(NEXTING_DOWNLINK_UUID);
static struct bt_uuid_128 uplink_uuid = BT_UUID_INIT_128(NEXTING_UPLINK_UUID);
static struct bt_uuid_128 device_info_uuid = BT_UUID_INIT_128(NEXTING_DEVICE_INFO_UUID);

static uint8_t stream_storage[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES];
static nexting_device_stream_t stream;
static nexting_device_state_t approval_state;
static struct bt_conn *current_connection;
static bool notifications_enabled;
static char uplink_frame[UPLINK_FRAME_CAPACITY];
static char uplink_chunk[UPLINK_FRAME_CAPACITY];
static size_t uplink_frame_length;
static size_t uplink_frame_offset;
static size_t uplink_chunk_length;
static uint32_t uplink_generation = 1U;
static bool uplink_notification_in_flight;
static bool uplink_resync_required;
static bool uplink_frame_has_resync_prefix;
static bool bond_reset_in_progress;
static bool bond_reset_unpair_succeeded;
K_MUTEX_DEFINE(app_mutex);

static void bond_reset_work_handler(struct k_work *work);
static void bond_reset_led_off_handler(struct k_work *work);
static void advertising_work_handler(struct k_work *work);
K_WORK_DEFINE(bond_reset_work, bond_reset_work_handler);
K_WORK_DELAYABLE_DEFINE(bond_reset_led_off_work, bond_reset_led_off_handler);
K_WORK_DELAYABLE_DEFINE(advertising_work, advertising_work_handler);

static void set_pending_led(bool pending)
{
	(void)gpio_pin_set_dt(&pending_led, pending ? 1 : 0);
}

static void clear_uplink_locked(void)
{
	if (uplink_frame_length != 0U &&
	    (uplink_frame_offset != 0U || uplink_notification_in_flight)) {
		uplink_resync_required = true;
	}
	uplink_frame_length = 0U;
	uplink_frame_offset = 0U;
	uplink_chunk_length = 0U;
	uplink_notification_in_flight = false;
	uplink_frame_has_resync_prefix = false;
	uplink_generation++;
	if (uplink_generation == 0U) {
		uplink_generation = 1U;
	}
}

static void clear_volatile_state_locked(void)
{
	nexting_device_stream_reset(&stream);
	nexting_device_state_disconnect(&approval_state);
	clear_uplink_locked();
	uplink_resync_required = false;
	set_pending_led(false);
}

static ssize_t read_device_info(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buffer,
				uint16_t length, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buffer, length, offset, value,
				 strlen(value));
}

static void receive_message(const nexting_device_message_t *message, void *context)
{
	ARG_UNUSED(context);

	if (message->type == NEXTING_DEVICE_MESSAGE_PRESENT) {
		clear_uplink_locked();
		if (nexting_device_state_on_present(&approval_state, message,
						     k_uptime_get()) == NEXTING_DEVICE_OK) {
			set_pending_led(true);
			printk("Approval pending: %s\n", message->summary);
		}
		return;
	}

	if (message->type == NEXTING_DEVICE_MESSAGE_RESOLVED &&
	    nexting_device_state_on_resolved(&approval_state, message) == NEXTING_DEVICE_OK) {
		clear_uplink_locked();
		set_pending_led(false);
		printk("Approval resolved\n");
	}
}

static ssize_t write_downlink(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, const void *buffer,
			      uint16_t length, uint16_t offset, uint8_t flags)
{
	nexting_device_result_t result;

	ARG_UNUSED(attr);
	ARG_UNUSED(flags);
	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (bond_reset_in_progress || conn != current_connection) {
		k_mutex_unlock(&app_mutex);
		return BT_GATT_ERR(BT_ATT_ERR_AUTHORIZATION);
	}
	result = nexting_device_stream_push(&stream, buffer, length,
					 receive_message, NULL);
	k_mutex_unlock(&app_mutex);

	if (result == NEXTING_DEVICE_MESSAGE_TOO_LARGE) {
		printk("Dropped oversized message\n");
	} else if (result == NEXTING_DEVICE_BAD_MESSAGE) {
		printk("Dropped malformed message\n");
	}
	return length;
}

static void subscription_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	k_mutex_lock(&app_mutex, K_FOREVER);
	notifications_enabled = !bond_reset_in_progress &&
		(value == BT_GATT_CCC_NOTIFY);
	if (!notifications_enabled) {
		clear_volatile_state_locked();
	}
	k_mutex_unlock(&app_mutex);
}

BT_GATT_SERVICE_DEFINE(nexting_service,
	BT_GATT_PRIMARY_SERVICE(&service_uuid),
	BT_GATT_CHARACTERISTIC(&downlink_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_downlink, NULL),
	BT_GATT_CHARACTERISTIC(&uplink_uuid.uuid, BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(subscription_changed,
		BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_CHARACTERISTIC(&device_info_uuid.uuid, BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ, read_device_info, NULL, DEVICE_INFO_JSON),
);

static const struct bt_data advertising_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, NEXTING_SERVICE_UUID),
};

static const struct bt_data scan_response[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static int start_advertising(void)
{
	const int error = bt_le_adv_start(NEXTING_CONNECTABLE_ADV_PARAMS,
					  advertising_data,
					  ARRAY_SIZE(advertising_data), scan_response,
					  ARRAY_SIZE(scan_response));

	return error == -EALREADY ? 0 : error;
}

static void schedule_advertising(void)
{
	(void)k_work_reschedule(&advertising_work, ADVERTISING_RETRY_DELAY);
}

static void advertising_work_handler(struct k_work *work)
{
	bool can_advertise;
	int error;

	ARG_UNUSED(work);
	k_mutex_lock(&app_mutex, K_FOREVER);
	can_advertise = !bond_reset_in_progress && current_connection == NULL;
	k_mutex_unlock(&app_mutex);
	if (!can_advertise) {
		return;
	}

	error = start_advertising();
	if (error != 0) {
		printk("Advertising start deferred: %d\n", error);
		(void)k_work_reschedule(&advertising_work,
					ADVERTISING_RETRY_DELAY);
	}
}

static void bond_reset_led_off_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (approval_state.phase == NEXTING_DEVICE_PHASE_IDLE) {
		set_pending_led(false);
	}
	k_mutex_unlock(&app_mutex);
}

static bool complete_bond_reset_locked(void)
{
	if (!bond_reset_in_progress || !bond_reset_unpair_succeeded ||
	    current_connection != NULL) {
		return false;
	}
	bond_reset_in_progress = false;
	bond_reset_unpair_succeeded = false;
	set_pending_led(true);
	return true;
}

static void publish_bond_reset_success(void)
{
	printk("All bonds cleared; ready to pair again\n");
	(void)k_work_reschedule(&bond_reset_led_off_work, K_SECONDS(1));
	schedule_advertising();
}

static void bond_reset_work_handler(struct k_work *work)
{
	struct bt_conn *connection = NULL;
	bool reset_complete;
	int error;

	ARG_UNUSED(work);
	k_mutex_lock(&app_mutex, K_FOREVER);
	if (current_connection != NULL) {
		connection = bt_conn_ref(current_connection);
	}
	bond_reset_in_progress = true;
	bond_reset_unpair_succeeded = false;
	notifications_enabled = false;
	clear_volatile_state_locked();
	k_mutex_unlock(&app_mutex);

	error = bt_le_adv_stop();
	if (error != 0 && error != -EALREADY) {
		printk("Advertising stop failed during bond reset: %d\n", error);
	}

	if (connection != NULL) {
		error = bt_conn_disconnect(connection,
					   BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (error != 0 && error != -ENOTCONN) {
			printk("Bond reset disconnect failed: %d\n", error);
		}
		bt_conn_unref(connection);
	}

	error = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	if (error != 0) {
		printk("Bond reset failed: %d\n", error);
		return;
	}

	k_mutex_lock(&app_mutex, K_FOREVER);
	bond_reset_unpair_succeeded = true;
	reset_complete = complete_bond_reset_locked();
	k_mutex_unlock(&app_mutex);
	if (reset_complete) {
		publish_bond_reset_success();
	}
}

static void connected(struct bt_conn *conn, uint8_t error)
{
	bool reject_connection;
	int disconnect_error;
	int security_error;

	if (error != 0U) {
		printk("Connection failed: %u\n", error);
		schedule_advertising();
		return;
	}

	k_mutex_lock(&app_mutex, K_FOREVER);
	reject_connection = bond_reset_in_progress;
	if (!reject_connection) {
		if (current_connection != NULL) {
			bt_conn_unref(current_connection);
		}
		current_connection = bt_conn_ref(conn);
		notifications_enabled = false;
		clear_volatile_state_locked();
	}
	k_mutex_unlock(&app_mutex);
	if (reject_connection) {
		security_error = bt_conn_disconnect(
			conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (security_error != 0 && security_error != -ENOTCONN) {
			printk("Connection rejection failed during bond reset: %d\n",
			       security_error);
		}
		return;
	}

	security_error = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (security_error != 0) {
		printk("Could not request encrypted bonding: %d\n", security_error);
		disconnect_error = bt_conn_disconnect(
			conn, BT_HCI_ERR_AUTH_FAIL);
		if (disconnect_error != 0 && disconnect_error != -ENOTCONN) {
			printk("Security request disconnect failed: %d\n",
			       disconnect_error);
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	bool reset_complete;
	bool should_resume_advertising;

	k_mutex_lock(&app_mutex, K_FOREVER);
	clear_volatile_state_locked();
	notifications_enabled = false;
	if (current_connection == conn) {
		bt_conn_unref(current_connection);
		current_connection = NULL;
	}
	reset_complete = complete_bond_reset_locked();
	should_resume_advertising = !bond_reset_in_progress && !reset_complete;
	k_mutex_unlock(&app_mutex);
	printk("Disconnected: %u\n", reason);
	if (reset_complete) {
		publish_bond_reset_success();
		return;
	}
	if (!should_resume_advertising) {
		return;
	}
	schedule_advertising();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err error)
{
	int disconnect_error;

	printk("Security level %u, error %u\n", level, error);
	if (error == BT_SECURITY_ERR_SUCCESS && level >= BT_SECURITY_L2) {
		return;
	}
	disconnect_error = bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	if (disconnect_error != 0 && disconnect_error != -ENOTCONN) {
		printk("Unsecured peer disconnect failed: %d\n", disconnect_error);
	}
}

BT_CONN_CB_DEFINE(connection_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static bool queue_uplink_locked(const nexting_device_message_t *message)
{
	size_t encoded_length = 0U;
	const size_t prefix_length = uplink_resync_required ? 1U : 0U;

	if (current_connection == NULL || !notifications_enabled ||
	    uplink_frame_length != 0U || uplink_notification_in_flight) {
		return false;
	}
	if (uplink_resync_required) {
		uplink_frame[0] = '\n';
	}
	if (nexting_device_encode(message, uplink_frame + prefix_length,
				  sizeof(uplink_frame) - prefix_length,
				  &encoded_length) != NEXTING_DEVICE_OK) {
		uplink_frame_length = 0U;
		uplink_frame_has_resync_prefix = false;
		return false;
	}
	uplink_frame_length = prefix_length + encoded_length;
	uplink_frame_offset = 0U;
	uplink_frame_has_resync_prefix = uplink_resync_required;
	approval_state.next_retry_ms = UINT64_MAX;
	return true;
}

static void notification_sent(struct bt_conn *conn, void *user_data)
{
	const uint32_t generation = (uint32_t)POINTER_TO_UINT(user_data);

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (generation != uplink_generation || !uplink_notification_in_flight ||
	    conn != current_connection) {
		k_mutex_unlock(&app_mutex);
		return;
	}

	uplink_notification_in_flight = false;
	if (uplink_frame_has_resync_prefix && uplink_frame_offset == 0U) {
		uplink_resync_required = false;
		uplink_frame_has_resync_prefix = false;
	}
	uplink_frame_offset += uplink_chunk_length;
	uplink_chunk_length = 0U;
	if (uplink_frame_offset >= uplink_frame_length) {
		uplink_frame_length = 0U;
		uplink_frame_offset = 0U;
		if (approval_state.phase == NEXTING_DEVICE_PHASE_WAITING_RESOLUTION) {
			approval_state.next_retry_ms =
				(uint64_t)k_uptime_get() + NEXTING_DEVICE_ANSWER_RETRY_MS;
		}
	}
	k_mutex_unlock(&app_mutex);
}

static void pump_uplink(void)
{
	struct bt_gatt_notify_params params = { 0 };
	struct bt_conn *connection;
	uint32_t generation;
	int error;

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (current_connection == NULL || !notifications_enabled ||
	    uplink_frame_length == 0U || uplink_notification_in_flight) {
		k_mutex_unlock(&app_mutex);
		return;
	}

	const uint16_t payload_bytes = bt_gatt_get_mtu(current_connection) - 3U;
	const size_t remaining = uplink_frame_length - uplink_frame_offset;
	uplink_chunk_length = MIN(remaining, (size_t)payload_bytes);
	memcpy(uplink_chunk, uplink_frame + uplink_frame_offset,
	       uplink_chunk_length);
	uplink_notification_in_flight = true;
	generation = uplink_generation;
	connection = bt_conn_ref(current_connection);
	params.attr = &nexting_service.attrs[4];
	params.data = uplink_chunk;
	params.len = (uint16_t)uplink_chunk_length;
	params.func = notification_sent;
	params.user_data = UINT_TO_POINTER(generation);
	k_mutex_unlock(&app_mutex);

	error = bt_gatt_notify_cb(connection, &params);
	bt_conn_unref(connection);
	if (error == 0) {
		return;
	}

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (generation == uplink_generation && uplink_notification_in_flight) {
		uplink_notification_in_flight = false;
		uplink_chunk_length = 0U;
	}
	k_mutex_unlock(&app_mutex);
	printk("Answer notification deferred: %d\n", error);
}

static void choose(nexting_device_choice_t choice)
{
	nexting_device_message_t answer = { 0 };

	k_mutex_lock(&app_mutex, K_FOREVER);
	if (nexting_device_state_choose(&approval_state, choice, k_uptime_get(),
					&answer) == NEXTING_DEVICE_OK) {
		(void)queue_uplink_locked(&answer);
	}
	k_mutex_unlock(&app_mutex);
}

static int configure_gpio(void)
{
	if (!gpio_is_ready_dt(&allow_button) || !gpio_is_ready_dt(&deny_button) ||
	    !gpio_is_ready_dt(&pending_led)) {
		return -ENODEV;
	}
	if (gpio_pin_configure_dt(&allow_button, GPIO_INPUT) != 0 ||
	    gpio_pin_configure_dt(&deny_button, GPIO_INPUT) != 0 ||
	    gpio_pin_configure_dt(&pending_led, GPIO_OUTPUT_INACTIVE) != 0) {
		return -EIO;
	}
	return 0;
}

int main(void)
{
	bool allow_was_pressed = false;
	bool deny_was_pressed = false;
	bool chord_active = false;
	bool bond_reset_submitted = false;
	int64_t both_pressed_since = -1;
	const int64_t bond_reset_hold_ms =
		k_ticks_to_ms_floor64(K_SECONDS(3).ticks);
	int error;

	nexting_device_stream_init(&stream, stream_storage, sizeof(stream_storage));
	nexting_device_state_init(&approval_state);
	if (configure_gpio() != 0) {
		printk("GPIO configuration failed\n");
		return 0;
	}

	error = bt_enable(NULL);
	if (error != 0) {
		printk("Bluetooth initialization failed: %d\n", error);
		return 0;
	}
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		error = settings_load();
		if (error != 0) {
			printk("Bond settings load failed: %d\n", error);
		}
	}
	error = start_advertising();
	if (error != 0) {
		printk("Advertising failed: %d\n", error);
		return 0;
	}
	printk("Nexting Device Protocol Experimental 0.2 ready on %s\n", CONFIG_BOARD);

	for (;;) {
		const bool allow_pressed = gpio_pin_get_dt(&allow_button) > 0;
		const bool deny_pressed = gpio_pin_get_dt(&deny_button) > 0;
		const bool both_pressed = allow_pressed && deny_pressed;
		const int64_t now_ms = k_uptime_get();
		nexting_device_message_t retry = { 0 };

		if (both_pressed) {
			if (!chord_active) {
				chord_active = true;
				bond_reset_submitted = false;
				both_pressed_since = now_ms;
			} else if (!bond_reset_submitted &&
				   now_ms - both_pressed_since >= bond_reset_hold_ms) {
				bond_reset_submitted = true;
				(void)k_work_submit(&bond_reset_work);
			}
		} else if (chord_active) {
			if (!allow_pressed && !deny_pressed) {
				if (!bond_reset_submitted) {
					printk("Short two-button press ignored\n");
				}
				chord_active = false;
				both_pressed_since = -1;
			}
		} else if (allow_was_pressed && !allow_pressed) {
			choose(NEXTING_DEVICE_CHOICE_ALLOW);
		} else if (deny_was_pressed && !deny_pressed) {
			choose(NEXTING_DEVICE_CHOICE_DENY);
		}
		allow_was_pressed = allow_pressed;
		deny_was_pressed = deny_pressed;

		k_mutex_lock(&app_mutex, K_FOREVER);
		if (nexting_device_state_tick(&approval_state, (uint64_t)now_ms)) {
			clear_uplink_locked();
			set_pending_led(false);
		} else if (nexting_device_state_retry_answer(&approval_state,
							(uint64_t)now_ms, &retry)) {
			(void)queue_uplink_locked(&retry);
		}
		k_mutex_unlock(&app_mutex);
		pump_uplink();

		k_sleep(K_MSEC(20));
	}
	return 0;
}
