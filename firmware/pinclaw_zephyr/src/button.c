#include "button.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include "led.h"
#include "mic.h"
// #include "sdcard.h"
#include "speaker.h"
#include "transport.h"
#include "wdog_facade.h"
LOG_MODULE_REGISTER(button, CONFIG_LOG_DEFAULT_LEVEL);

bool is_off = false;
static void button_ccc_config_changed_handler(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t button_data_read_characteristic(struct bt_conn *conn,
                                               const struct bt_gatt_attr *attr,
                                               void *buf,
                                               uint16_t len,
                                               uint16_t offset);
static struct gpio_callback button_cb_data;

static struct bt_uuid_128 button_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x23BA7924, 0x0000, 0x1000, 0x7450, 0x346EAC492E92));
static struct bt_uuid_128 button_characteristic_data_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x23BA7925, 0x0000, 0x1000, 0x7450, 0x346EAC492E92));

static struct bt_gatt_attr button_service_attr[] = {
    BT_GATT_PRIMARY_SERVICE(&button_uuid),
    BT_GATT_CHARACTERISTIC(&button_characteristic_data_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           button_data_read_characteristic,
                           NULL,
                           NULL),
    BT_GATT_CCC(button_ccc_config_changed_handler, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service button_service = BT_GATT_SERVICE(button_service_attr);

static void button_ccc_config_changed_handler(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == BT_GATT_CCC_NOTIFY) {
        LOG_INF("Client subscribed for notifications");
    } else if (value == 0) {
        LOG_INF("Client unsubscribed from notifications");
    } else {
        LOG_ERR("Invalid CCC value: %u", value);
    }
}
// Pinclaw: D5 only, active LOW with internal pullup (no D4 power needed)
struct gpio_dt_spec d5_pin_input = {.port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
                                    .pin = 5,
                                    .dt_flags = 0};

static bool was_pressed = false;

//
// button
//
void button_pressed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int temp = gpio_pin_get_raw(dev, d5_pin_input.pin);
    // Pinclaw: active LOW (pressed = 0, released = 1)
    was_pressed = (temp == 0);
}
#define BUTTON_CHECK_INTERVAL 40 // 0.04 seconds, 25 Hz

void check_button_level(struct k_work *work_item);

K_WORK_DELAYABLE_DEFINE(button_work, check_button_level);

#define DEFAULT_STATE 0
#define SINGLE_TAP 1
#define DOUBLE_TAP 2
#define LONG_TAP 3
#define BUTTON_PRESS 4
#define BUTTON_RELEASE 5

// 4 is button down, 5 is button up
static FSM_STATE_T current_button_state = IDLE;
static uint32_t inc_count_1 = 0;
static uint32_t inc_count_0 = 0;

static int final_button_state[2] = {0, 0};
const static int threshold = 10;

static void reset_count()
{
    inc_count_0 = 0;
    inc_count_1 = 0;
}
static inline void notify_press()
{
    final_button_state[0] = BUTTON_PRESS;
    LOG_INF("Button pressed");
    struct bt_conn *conn = get_current_connection();
    if (conn != NULL) {
        bt_gatt_notify(conn, &button_service.attrs[1], &final_button_state, sizeof(final_button_state));
    }
}

static inline void notify_unpress()
{
    final_button_state[0] = BUTTON_RELEASE;
    LOG_INF("Button released");
    struct bt_conn *conn = get_current_connection();
    if (conn != NULL) {
        bt_gatt_notify(conn, &button_service.attrs[1], &final_button_state, sizeof(final_button_state));
    }
}

static inline void notify_tap()
{
    LOG_INF("Button single tap -> PLAY command");
    struct bt_conn *conn = get_current_connection();
    if (conn != NULL) {
        // Send PLAY command (0x20) on ABD text characteristic — same as iOS expects
        extern struct bt_gatt_service audio_service;
        uint8_t play_cmd = 0x20;
        bt_gatt_notify(conn, &audio_service.attrs[3], &play_cmd, 1);  // attrs[3] = text/ABD char
    }
}

static inline void notify_double_tap()
{
    final_button_state[0] = DOUBLE_TAP; // button press
    LOG_INF("Button double tap");
    struct bt_conn *conn = get_current_connection();
    if (conn != NULL) {
        bt_gatt_notify(conn, &button_service.attrs[1], &final_button_state, sizeof(final_button_state));
    }
}

static inline void notify_long_tap()
{
    final_button_state[0] = LONG_TAP; // button press
    LOG_INF("Button long tap");
    struct bt_conn *conn = get_current_connection();
    if (conn != NULL) {
        bt_gatt_notify(conn, &button_service.attrs[1], &final_button_state, sizeof(final_button_state));
    }
}

#define BUTTON_PRESSED 1
#define BUTTON_RELEASED 0

#define TAP_THRESHOLD 300     // 300 ms for single tap
#define DOUBLE_TAP_WINDOW 600 // 600 ms maximum for double-tap
#define LONG_PRESS_TIME 500  // 3000 ms for long press (power off)

typedef enum {
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_SINGLE_TAP,
    BUTTON_EVENT_DOUBLE_TAP,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_RELEASE
} ButtonEvent;

static uint32_t current_time = 0;
static uint32_t btn_press_start_time;
static uint32_t btn_release_time;
static uint32_t btn_last_tap_time;
static bool btn_is_pressed;

static u_int8_t btn_last_event = BUTTON_EVENT_NONE;

void check_button_level(struct k_work *work_item)
{
    // Poll D5 directly (active LOW with pullup)
    int raw = gpio_pin_get_raw(d5_pin_input.port, d5_pin_input.pin);
    bool pressed = (raw == 0);
    
    static bool was_btn_pressed = false;
    static int64_t press_start_time = 0;
    
    extern volatile bool recording_active;
    extern uint16_t opus_seq_no;
    extern struct bt_gatt_service audio_service;
    
    // Button just pressed
    if (pressed && !was_btn_pressed) {
        was_btn_pressed = true;
        press_start_time = k_uptime_get();
        
        // Start recording immediately
        recording_active = true;
        opus_seq_no = 0;
        struct bt_conn *conn = get_current_connection();
        if (conn) {
            uint8_t start_pkt[6] = {0x01, 0x14, 0x00, 0x00, 0x00, 0x00};
            bt_gatt_notify(conn, &audio_service.attrs[1], start_pkt, 6);
        }
        LOG_INF("Button pressed — recording started");
        set_led_red(true);  // LED feedback
    }
    
    // Button just released
    if (!pressed && was_btn_pressed) {
        was_btn_pressed = false;
        int64_t duration = k_uptime_get() - press_start_time;
        
        // Stop recording
        recording_active = false;
        struct bt_conn *conn = get_current_connection();
        if (conn) {
            uint8_t end_pkt[5];
            end_pkt[0] = 0x03;
            end_pkt[1] = (opus_seq_no >> 24) & 0xFF;
            end_pkt[2] = (opus_seq_no >> 16) & 0xFF;
            end_pkt[3] = (opus_seq_no >> 8) & 0xFF;
            end_pkt[4] = opus_seq_no & 0xFF;
            bt_gatt_notify(conn, &audio_service.attrs[1], end_pkt, 5);
        }
        
        set_led_red(false);  // LED off
        
        if (duration < 500) {
            // Short press (< 0.5s) — cancel recording, execute PLAY
            LOG_INF("Short press (%lld ms) — PLAY command", duration);
            if (conn) {
                uint8_t play_cmd = 0x20;
                bt_gatt_notify(conn, &audio_service.attrs[3], &play_cmd, 1);
            }
        } else {
            // Long press (>= 0.5s) — normal recording stop
            LOG_INF("Long press (%lld ms) — recording stopped", duration);
        }
    }
    
    k_work_reschedule(&button_work, K_MSEC(BUTTON_CHECK_INTERVAL));
}

// @deprecated
// #define LONG_PRESS_INTERVAL 25
// #define SINGLE_PRESS_INTERVAL 2
// void check_button_level_2(struct k_work *work_item)
//{
//     //insert the current button state here
//    int state_ = was_pressed ? 1 : 0;
//    if (current_button_state == IDLE)
//    {
//        if (state_ == 0)
//        {
//            //Do nothing!
//        }
//        else if (state_ == 1)
//        {
//            //Also do nothing, but transition to the next state
//            notify_press();
//            current_button_state = ONE_PRESS;
//            if (is_off)
//           {
//             is_off = false;
//             bt_on();
//             play_haptic_milli(50);
//           }
//        }
//    }
//
//    else if (current_button_state == ONE_PRESS)
//    {
//        if (state_ == 0)
//        {
//
//            if(inc_count_0 == 0)
//            {
//                notify_unpress();
//            }
//            inc_count_0++; //button is unpressed
//            if (inc_count_0 > SINGLE_PRESS_INTERVAL)
//            {
//                //If button is not pressed for a little while.......
//                //transition to Two_press. button could be a single or double tap
//                current_button_state = TWO_PRESS;
//                reset_count();
//            }
//        }
//        if (state_ == 1)
//        {
//            inc_count_1++; //button is pressed
//
//            if (inc_count_1 > LONG_PRESS_INTERVAL)
//            {
//                //If button is pressed for a long time.......
//                notify_long_tap();
//                //play_haptic_milli(10);
//                //Fire the long mode notify and enter a grace period
//                //turn off herre
//                // TODO: FIXME
//                //if(!from_wakeup)
//                //{
//                //    is_off = !is_off;
//                //}
//                //else
//                //{
//                //    from_wakeup = false;
//                //}
//                //if (is_off)
//                //{
//                //    bt_off();
//                //    turnoff_all();
//                //}
//                current_button_state = GRACE;
//                reset_count();
//            }
//
//        }
//
//    }
//
//    else if (current_button_state == TWO_PRESS)
//    {
//        if (state_ == 0)
//        {
//            if (inc_count_1 > 0)
//            { // if button has been pressed......
//                notify_unpress();
//                notify_double_tap();
//
//                //Fire the notify and enter a grace period
//                current_button_state = GRACE;
//                reset_count();
//            }
//             //single button press
//            else if (inc_count_0 > 10)
//            {
//                notify_tap(); //Fire the notify and enter a grace period
//                if(!from_wakeup)
//                {
//                    is_off = !is_off;
//                }
//                else
//                {
//                    from_wakeup = false;
//                }
//                //Fire the notify and enter a grace period
//                if (is_off)
//                {
//                    bt_off();
//                    turnoff_all();
//                }
//                current_button_state = GRACE;
//                reset_count();
//            }
//            else
//            {
//                inc_count_0++; //not pressed
//            }
//        }
//        else if (state_ == 1 )
//        {
//            if (inc_count_1 == 0)
//            {
//                notify_press();
//                inc_count_1++;
//            }
//            if (inc_count_1 > threshold)
//            {
//                notify_long_tap();
//                //play_haptic_milli(10);
//                // TODO: FIXME
//                //if(!from_wakeup)
//                //{
//                //    is_off = !is_off;
//                //}
//                //else
//                //{
//                //    from_wakeup = false;
//                //}
//                ////Fire the notify and enter a grace period
//                //if (is_off)
//                //{
//                //    bt_off();
//                //    turnoff_all();
//                //}
//                current_button_state = GRACE;
//                reset_count();
//            }
//        }
//    }
//
//    else if (current_button_state == GRACE)
//    {
//        if (state_ == 0)
//        {
//            if (inc_count_0 == 0 && (inc_count_1 > 0))
//            {
//                notify_unpress();
//            }
//            inc_count_0++;
//            if (inc_count_0 > 1)
//            {
//                current_button_state = IDLE;
//                reset_count();
//            }
//        }
//        else if (state_ == 1)
//        {
//            inc_count_1++;
//        }
//    }
//    k_work_reschedule(&button_work, K_MSEC(BUTTON_CHECK_INTERVAL));
//}

static ssize_t button_data_read_characteristic(struct bt_conn *conn,
                                               const struct bt_gatt_attr *attr,
                                               void *buf,
                                               uint16_t len,
                                               uint16_t offset)
{
    LOG_INF("button_data_read_characteristic");
    LOG_PRINTK("was_pressed: %d\n", final_button_state[0]);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &final_button_state, sizeof(final_button_state));
}

int button_init()
{
    // Pinclaw: D5 only, with internal pullup
    if (gpio_is_ready_dt(&d5_pin_input)) {
        LOG_INF("D5 Pin ready");
    } else {
        LOG_ERR("D5 Pin not ready");
        return -1;
    }

    int err2 = gpio_pin_configure_dt(&d5_pin_input, GPIO_INPUT | GPIO_PULL_UP);

    if (err2 != 0) {
        LOG_ERR("Error setting up D5 Pin");
        return -1;
    } else {
        LOG_INF("D5 ready");
    }
    // GPIO_INT_LEVEL_INACTIVE
    err2 = gpio_pin_interrupt_configure(d5_pin_input.port, d5_pin_input.pin, GPIO_INT_EDGE_BOTH);

    if (err2 != 0) {
        LOG_ERR("D5 unable to detect button presses");
        return -1;
    } else {
        LOG_INF("D5 ready to detect button presses");
    }

    gpio_init_callback(&button_cb_data, button_pressed_callback, BIT(d5_pin_input.pin));
    gpio_add_callback(d5_pin_input.port, &button_cb_data);

    LOG_INF("D5 callback registered on pin %d", d5_pin_input.pin);
    // Debug: read current pin state
    int d5_val = gpio_pin_get_raw(d5_pin_input.port, d5_pin_input.pin);
    LOG_INF("D5 current value: %d (should be 1 if not pressed with pullup)", d5_val);

    return 0;
}

void activate_button_work()
{
    k_work_schedule(&button_work, K_MSEC(BUTTON_CHECK_INTERVAL));
}

void register_button_service()
{
    bt_gatt_service_register(&button_service);
}

FSM_STATE_T get_current_button_state()
{
    return current_button_state;
}

void turnoff_all()
{

    mic_off();
    // sd_off(); // SD disabled
    speaker_off();
    accel_off();
    play_haptic_milli(50);
    k_msleep(100);
    set_led_blue(false);
    set_led_red(false);
    set_led_green(false);
    gpio_remove_callback(d5_pin_input.port, &button_cb_data);
    gpio_pin_interrupt_configure_dt(&d5_pin_input, GPIO_INT_LEVEL_INACTIVE);

    // Disable watchdog before entering system off
    int rc = watchdog_deinit();
    if (rc < 0) {
        LOG_ERR("Failed to deinitialize watchdog (%d)", rc);
    }

    // maybe save something here to indicate success. next time the button is pressed we should know about it
    NRF_USBD->INTENCLR = 0xFFFFFFFF;
    NRF_POWER->SYSTEMOFF = 1;
}

void force_button_state(FSM_STATE_T state)
{
    current_button_state = state;
}
