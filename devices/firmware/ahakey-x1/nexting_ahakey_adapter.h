#ifndef NEXTING_AHAKEY_ADAPTER_H
#define NEXTING_AHAKEY_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ahakey_proto.h"
#include "nexting_ahakey_audio.h"
#include "nexting_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mapping layer between the portable Nexting C99 state machine and the
 * AhaKey X1 hardware concepts. The adapter is transport-agnostic: a native
 * X1 firmware port feeds it Nexting wire bytes from the GATT downlink and
 * renders to its own LED/OLED, while a bridge dongle feeds it the same bytes
 * and forwards the rendered states to a stock keyboard over the published
 * AhaKey 0x7340 protocol.
 *
 * Stock-firmware key mapping (configure with the vendor tool):
 *   F18 (0x6D) -> approval Allow, F19 (0x6E) -> approval Deny,
 *   F13-F16 (0x68-0x6B) -> keys/1 slots 0-3.
 * The physical lever reports through key slot NEXTING_AHAKEY_LEVER_SLOT:
 * press when it leaves the rest position, release when it returns.
 */

#define NEXTING_AHAKEY_STREAM_CAPACITY \
  (NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1U)
#define NEXTING_AHAKEY_LEVER_SLOT 7U
#define NEXTING_AHAKEY_KEY_SLOT_COUNT 4U

#define NEXTING_AHAKEY_HID_F13 0x68U
#define NEXTING_AHAKEY_HID_F14 0x69U
#define NEXTING_AHAKEY_HID_F15 0x6AU
#define NEXTING_AHAKEY_HID_F16 0x6BU
#define NEXTING_AHAKEY_HID_ALLOW 0x6DU /* F18 */
#define NEXTING_AHAKEY_HID_DENY 0x6EU  /* F19 */

typedef uint64_t (*nexting_ahakey_now_ms_fn)(void *context);
typedef void (*nexting_ahakey_write_frame_fn)(const uint8_t *bytes,
                                              size_t length, void *context);
typedef void (*nexting_ahakey_render_approval_fn)(
    const nexting_device_state_t *state, void *context);
typedef void (*nexting_ahakey_render_led_fn)(ahakey_state_t state,
                                             void *context);
typedef void (*nexting_ahakey_render_text_fn)(
    const nexting_device_text_payload_t *text, void *context);

typedef struct {
  nexting_device_state_t approval;
  nexting_device_status_state_t status;
  nexting_device_stream_t stream;
  uint8_t stream_storage[NEXTING_AHAKEY_STREAM_CAPACITY];
  ahakey_state_t led;
  uint8_t lever_state;
  bool lever_known;
  uint32_t next_sequence;
  /* Last Host keymap replacement; keys/1 events emit only for enabled slots. */
  bool key_enabled[NEXTING_DEVICE_KEYS_MAX];
  nexting_ahakey_audio_t audio;
  nexting_ahakey_now_ms_fn now_ms;
  nexting_ahakey_write_frame_fn write_frame;
  nexting_ahakey_render_approval_fn render_approval;
  nexting_ahakey_render_led_fn render_led;
  nexting_ahakey_render_text_fn render_text;
  void *context;
} nexting_ahakey_t;

void nexting_ahakey_init(nexting_ahakey_t *adapter,
                         nexting_ahakey_now_ms_fn now_ms,
                         nexting_ahakey_write_frame_fn write_frame,
                         nexting_ahakey_render_approval_fn render_approval,
                         nexting_ahakey_render_led_fn render_led,
                         nexting_ahakey_render_text_fn render_text,
                         void *context);

/* Feed Nexting wire bytes (JSONL) from the Host downlink. */
nexting_device_result_t nexting_ahakey_receive(nexting_ahakey_t *adapter,
                                               const uint8_t *bytes,
                                               size_t length);

/* Answer the pending approval; writes the answer frame on success. */
nexting_device_result_t nexting_ahakey_choose(nexting_ahakey_t *adapter,
                                              nexting_device_choice_t choice);

/*
 * Route one stock-firmware HID usage ID. F18/F19 answer the pending
 * approval; F13-F16 emit keys/1 events for slots 0-3 (only when the Host
 * keymap has declared and enabled that slot); anything else is ignored and
 * returns false.
 */
bool nexting_ahakey_hid_key(nexting_ahakey_t *adapter, uint8_t usage_id,
                            bool pressed);

/*
 * Report the polled lever (SwitchState from the AhaKey status response).
 * The first report only establishes the baseline; later changes emit a
 * keys/1 event on NEXTING_AHAKEY_LEVER_SLOT when the Host keymap has
 * declared and enabled that slot.
 */
bool nexting_ahakey_lever(nexting_ahakey_t *adapter, uint8_t switch_state);

/*
 * Bind the real device microphone and BLE audio-notify port. The App receives
 * this microphone's PCM; these APIs never start a phone microphone.
 */
bool nexting_ahakey_bind_device_audio(
    nexting_ahakey_t *adapter, size_t maximum_packet_bytes,
    nexting_ahakey_audio_write_packet_fn write_packet,
    nexting_ahakey_audio_hw_start_fn hardware_start,
    nexting_ahakey_audio_hw_stop_fn hardware_stop,
    nexting_ahakey_audio_hw_cancel_fn hardware_cancel,
    nexting_ahakey_audio_render_state_fn render_state, void *audio_context);
bool nexting_ahakey_voice_pressed(nexting_ahakey_t *adapter);
bool nexting_ahakey_voice_released(nexting_ahakey_t *adapter);
bool nexting_ahakey_voice_cancel(nexting_ahakey_t *adapter);
bool nexting_ahakey_on_pcm(
    nexting_ahakey_t *adapter,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]);
void nexting_ahakey_on_audio_hardware_error(nexting_ahakey_t *adapter);

void nexting_ahakey_tick(nexting_ahakey_t *adapter);
void nexting_ahakey_disconnect(nexting_ahakey_t *adapter);

/*
 * Resolve the LED state the hardware should show right now: a pending
 * approval outranks status (the approval channel renders on the same
 * strip), otherwise the lowest-numbered occupied status slot wins and
 * higher slots are ignored, per the status/1 rendering rule.
 */
ahakey_state_t nexting_ahakey_resolve_led(const nexting_ahakey_t *adapter);

#ifdef __cplusplus
}
#endif

#endif
