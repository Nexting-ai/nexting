#include "nexting_ahakey_adapter.h"

#include <string.h>

static uint64_t adapter_now_ms(const nexting_ahakey_t *adapter) {
  if (adapter == NULL || adapter->now_ms == NULL)
    return 0;
  return adapter->now_ms(adapter->context);
}

static uint64_t audio_now_ms(void *context) {
  return adapter_now_ms((const nexting_ahakey_t *)context);
}

static void audio_write_control(const nexting_device_message_t *message,
                                void *context);

static ahakey_state_t led_for_agent_state(nexting_device_agent_state_t state) {
  switch (state) {
  case NEXTING_DEVICE_AGENT_STATE_NEEDS_INPUT:
    return AHAKEY_STATE_PERMISSION_REQUEST;
  case NEXTING_DEVICE_AGENT_STATE_WORKING:
  case NEXTING_DEVICE_AGENT_STATE_THINKING:
    return AHAKEY_STATE_PRE_TOOL_USE;
  case NEXTING_DEVICE_AGENT_STATE_COMPLETE:
    return AHAKEY_STATE_TASK_COMPLETED;
  case NEXTING_DEVICE_AGENT_STATE_ERROR:
    return AHAKEY_STATE_NOTIFICATION;
  case NEXTING_DEVICE_AGENT_STATE_IDLE:
  case NEXTING_DEVICE_AGENT_STATE_NONE:
  default:
    return AHAKEY_STATE_STOP;
  }
}

/*
 * A pending approval outranks status on the shared strip (the approval
 * channel renders separately from status). Otherwise the lowest-numbered
 * occupied slot is rendered and the rest are ignored, per the status/1
 * rule for devices with fewer indicators than slots.
 */
ahakey_state_t nexting_ahakey_resolve_led(const nexting_ahakey_t *adapter) {
  if (adapter == NULL)
    return AHAKEY_STATE_STOP;
  if (adapter->approval.phase == NEXTING_DEVICE_PHASE_PENDING)
    return AHAKEY_STATE_PERMISSION_REQUEST;
  for (size_t i = 0; i < NEXTING_DEVICE_STATUS_MAX_AGENTS; ++i) {
    if (adapter->status.occupied[i])
      return led_for_agent_state(adapter->status.slots[i].state);
  }
  return AHAKEY_STATE_STOP;
}

static void render_led_if_changed(nexting_ahakey_t *adapter) {
  if (adapter == NULL)
    return;
  const ahakey_state_t resolved = nexting_ahakey_resolve_led(adapter);
  if (resolved == adapter->led)
    return;
  adapter->led = resolved;
  if (adapter->render_led != NULL)
    adapter->render_led(resolved, adapter->context);
}

static void render_approval(const nexting_ahakey_t *adapter) {
  if (adapter != NULL && adapter->render_approval != NULL)
    adapter->render_approval(&adapter->approval, adapter->context);
}

static void handle_message(const nexting_device_message_t *message,
                           void *context) {
  nexting_ahakey_t *adapter = (nexting_ahakey_t *)context;
  if (adapter == NULL || message == NULL)
    return;

  switch (message->type) {
  case NEXTING_DEVICE_MESSAGE_PRESENT:
    if (nexting_device_state_on_present(&adapter->approval, message,
                                        adapter_now_ms(adapter)) ==
        NEXTING_DEVICE_OK)
      render_approval(adapter);
    break;
  case NEXTING_DEVICE_MESSAGE_RESOLVED:
    if (nexting_device_state_on_resolved(&adapter->approval, message) ==
        NEXTING_DEVICE_OK)
      render_approval(adapter);
    break;
  case NEXTING_DEVICE_MESSAGE_STATUS:
    (void)nexting_device_status_on_message(&adapter->status, message);
    break;
  case NEXTING_DEVICE_MESSAGE_KEYMAP:
    /* keys/1 keymap is a full volatile replacement of the enabled set. */
    memset(adapter->key_enabled, 0, sizeof adapter->key_enabled);
    for (size_t i = 0; i < message->interaction.keymap.key_count; ++i) {
      const nexting_device_key_presentation_t *key =
          &message->interaction.keymap.keys[i];
      if (key->slot < NEXTING_DEVICE_KEYS_MAX && key->enabled)
        adapter->key_enabled[key->slot] = true;
    }
    break;
  case NEXTING_DEVICE_MESSAGE_TEXT:
    if (adapter->render_text != NULL)
      adapter->render_text(&message->interaction.text, adapter->context);
    break;
  case NEXTING_DEVICE_MESSAGE_AUDIO_CONFIG:
  case NEXTING_DEVICE_MESSAGE_AUDIO_CREDIT:
  case NEXTING_DEVICE_MESSAGE_AUDIO_CANCEL:
  case NEXTING_DEVICE_MESSAGE_AUDIO_STATE:
    (void)nexting_ahakey_audio_on_message(&adapter->audio, message);
    break;
  default:
    /* Answers and errors are host-facing; a device does not render them. */
    break;
  }
  render_led_if_changed(adapter);
}

/* Only answers and key events go uplink; both encode well under 256 bytes. */
#define NEXTING_AHAKEY_UPLINK_CAPACITY 256U

static void write_message(nexting_ahakey_t *adapter,
                          const nexting_device_message_t *message) {
  char wire[NEXTING_AHAKEY_UPLINK_CAPACITY];
  size_t wire_length = 0;
  if (adapter == NULL || message == NULL || adapter->write_frame == NULL ||
      nexting_device_encode(message, wire, sizeof wire, &wire_length) !=
          NEXTING_DEVICE_OK)
    return;
  adapter->write_frame((const uint8_t *)wire, wire_length, adapter->context);
}

static void audio_write_control(const nexting_device_message_t *message,
                                void *context) {
  write_message((nexting_ahakey_t *)context, message);
}

void nexting_ahakey_init(nexting_ahakey_t *adapter,
                         nexting_ahakey_now_ms_fn now_ms,
                         nexting_ahakey_write_frame_fn write_frame,
                         nexting_ahakey_render_approval_fn render_approval,
                         nexting_ahakey_render_led_fn render_led,
                         nexting_ahakey_render_text_fn render_text,
                         void *context) {
  if (adapter == NULL)
    return;
  memset(adapter, 0, sizeof *adapter);
  nexting_device_state_init(&adapter->approval);
  nexting_device_status_init(&adapter->status);
  nexting_device_stream_init(&adapter->stream, adapter->stream_storage,
                             sizeof adapter->stream_storage);
  adapter->led = AHAKEY_STATE_STOP;
  adapter->next_sequence = 1U;
  adapter->now_ms = now_ms;
  adapter->write_frame = write_frame;
  adapter->render_approval = render_approval;
  adapter->render_led = render_led;
  adapter->render_text = render_text;
  adapter->context = context;
  nexting_ahakey_audio_init(&adapter->audio, audio_now_ms,
                            audio_write_control, adapter);
}

nexting_device_result_t nexting_ahakey_receive(nexting_ahakey_t *adapter,
                                               const uint8_t *bytes,
                                               size_t length) {
  if (adapter == NULL || (bytes == NULL && length != 0))
    return NEXTING_DEVICE_BAD_MESSAGE;
  return nexting_device_stream_push(&adapter->stream, bytes, length,
                                    handle_message, adapter);
}

nexting_device_result_t nexting_ahakey_choose(nexting_ahakey_t *adapter,
                                              nexting_device_choice_t choice) {
  if (adapter == NULL || adapter->write_frame == NULL)
    return NEXTING_DEVICE_BAD_MESSAGE;
  nexting_device_message_t answer = {0};
  nexting_device_result_t result = nexting_device_state_choose(
      &adapter->approval, choice, adapter_now_ms(adapter), &answer);
  if (result == NEXTING_DEVICE_OK) {
    write_message(adapter, &answer);
    render_approval(adapter);
    render_led_if_changed(adapter);
  }
  return result;
}

static bool emit_key_event(nexting_ahakey_t *adapter, uint8_t slot,
                           nexting_device_gesture_t gesture) {
  /* A device emits no event for a disabled or undeclared slot. */
  if (adapter == NULL || adapter->write_frame == NULL ||
      slot >= NEXTING_DEVICE_KEYS_MAX || !adapter->key_enabled[slot])
    return false;
  nexting_device_message_t event = {0};
  event.type = NEXTING_DEVICE_MESSAGE_KEY_EVENT;
  event.interaction.slot = slot;
  event.interaction.gesture = gesture;
  event.interaction.sequence = adapter->next_sequence;
  adapter->next_sequence += 1U;
  write_message(adapter, &event);
  return true;
}

bool nexting_ahakey_hid_key(nexting_ahakey_t *adapter, uint8_t usage_id,
                            bool pressed) {
  if (adapter == NULL || !pressed)
    return false;
  if (usage_id == NEXTING_AHAKEY_HID_ALLOW)
    return nexting_ahakey_choose(adapter, NEXTING_DEVICE_CHOICE_ALLOW) ==
           NEXTING_DEVICE_OK;
  if (usage_id == NEXTING_AHAKEY_HID_DENY)
    return nexting_ahakey_choose(adapter, NEXTING_DEVICE_CHOICE_DENY) ==
           NEXTING_DEVICE_OK;
  if (usage_id >= NEXTING_AHAKEY_HID_F13 &&
      usage_id < NEXTING_AHAKEY_HID_F13 + NEXTING_AHAKEY_KEY_SLOT_COUNT)
    return emit_key_event(adapter, usage_id - NEXTING_AHAKEY_HID_F13,
                          NEXTING_DEVICE_GESTURE_PRESS);
  return false;
}

bool nexting_ahakey_lever(nexting_ahakey_t *adapter, uint8_t switch_state) {
  if (adapter == NULL)
    return false;
  if (!adapter->lever_known) {
    adapter->lever_known = true;
    adapter->lever_state = switch_state;
    return false;
  }
  if (switch_state == adapter->lever_state)
    return false;
  adapter->lever_state = switch_state;
  return emit_key_event(adapter, NEXTING_AHAKEY_LEVER_SLOT,
                        switch_state != 0U ? NEXTING_DEVICE_GESTURE_PRESS
                                           : NEXTING_DEVICE_GESTURE_RELEASE);
}

bool nexting_ahakey_bind_device_audio(
    nexting_ahakey_t *adapter, size_t maximum_packet_bytes,
    nexting_ahakey_audio_write_packet_fn write_packet,
    nexting_ahakey_audio_hw_start_fn hardware_start,
    nexting_ahakey_audio_hw_stop_fn hardware_stop,
    nexting_ahakey_audio_hw_cancel_fn hardware_cancel,
    nexting_ahakey_audio_render_state_fn render_state, void *audio_context) {
  if (adapter == NULL)
    return false;
  return nexting_ahakey_audio_bind(
      &adapter->audio, maximum_packet_bytes, write_packet, hardware_start,
      hardware_stop, hardware_cancel, render_state, audio_context);
}

bool nexting_ahakey_voice_pressed(nexting_ahakey_t *adapter) {
  return adapter != NULL && nexting_ahakey_audio_voice_pressed(&adapter->audio);
}

bool nexting_ahakey_voice_released(nexting_ahakey_t *adapter) {
  return adapter != NULL &&
         nexting_ahakey_audio_voice_released(&adapter->audio);
}

bool nexting_ahakey_voice_cancel(nexting_ahakey_t *adapter) {
  return adapter != NULL &&
         nexting_ahakey_audio_cancel_physical(&adapter->audio);
}

bool nexting_ahakey_on_pcm(
    nexting_ahakey_t *adapter,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]) {
  return adapter != NULL && nexting_ahakey_audio_on_pcm(&adapter->audio, samples);
}

void nexting_ahakey_on_audio_hardware_error(nexting_ahakey_t *adapter) {
  if (adapter != NULL)
    nexting_ahakey_audio_on_hardware_error(&adapter->audio);
}

void nexting_ahakey_tick(nexting_ahakey_t *adapter) {
  if (adapter == NULL)
    return;
  const uint64_t now_ms = adapter_now_ms(adapter);
  if (nexting_device_state_tick(&adapter->approval, now_ms)) {
    render_approval(adapter);
    render_led_if_changed(adapter);
  }
  nexting_device_message_t answer = {0};
  if (nexting_device_state_retry_answer(&adapter->approval, now_ms, &answer))
    write_message(adapter, &answer);
  nexting_ahakey_audio_tick(&adapter->audio);
}

void nexting_ahakey_disconnect(nexting_ahakey_t *adapter) {
  if (adapter == NULL)
    return;
  nexting_device_state_disconnect(&adapter->approval);
  nexting_device_status_disconnect(&adapter->status);
  nexting_device_stream_reset(&adapter->stream);
  nexting_ahakey_audio_disconnect(&adapter->audio);
  adapter->lever_known = false;
  /* The Host keymap is volatile: it is re-presented after reconnect. */
  memset(adapter->key_enabled, 0, sizeof adapter->key_enabled);
  adapter->led = AHAKEY_STATE_STOP;
  render_approval(adapter);
  if (adapter->render_led != NULL)
    adapter->render_led(AHAKEY_STATE_STOP, adapter->context);
}
