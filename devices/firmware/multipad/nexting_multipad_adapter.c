#include "nexting_multipad_adapter.h"

#include <string.h>

static uint64_t adapter_now_ms(const nexting_multipad_t *adapter) {
  if (adapter == NULL || adapter->now_ms == NULL)
    return 0;
  return adapter->now_ms(adapter->context);
}

static void render_approval(const nexting_multipad_t *adapter) {
  if (adapter != NULL && adapter->render_approval != NULL)
    adapter->render_approval(&adapter->approval, adapter->context);
}

static void render_status(const nexting_multipad_t *adapter) {
  if (adapter != NULL && adapter->render_status != NULL)
    adapter->render_status(&adapter->status, adapter->context);
}

static void handle_message(const nexting_device_message_t *message,
                           void *context) {
  nexting_multipad_t *adapter = (nexting_multipad_t *)context;
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
    if (nexting_device_status_on_message(&adapter->status, message) ==
        NEXTING_DEVICE_OK)
      render_status(adapter);
    break;
  default:
    /* Answers and errors are host-facing; a device does not render them. */
    break;
  }
}

static void write_answer(nexting_multipad_t *adapter,
                         const nexting_device_message_t *answer) {
  char wire[NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1U];
  size_t wire_length = 0;
  if (adapter == NULL || answer == NULL || adapter->write_frame == NULL ||
      nexting_device_encode(answer, wire, sizeof wire, &wire_length) !=
          NEXTING_DEVICE_OK)
    return;
  adapter->write_frame((const uint8_t *)wire, wire_length, adapter->context);
}

static bool contains_newline(const uint8_t *bytes, size_t length) {
  if (bytes == NULL)
    return false;
  for (size_t i = 0; i < length; ++i) {
    if (bytes[i] == '\n')
      return true;
  }
  return false;
}

void nexting_multipad_init(nexting_multipad_t *adapter,
                           nexting_multipad_now_ms_fn now_ms,
                           nexting_multipad_write_frame_fn write_frame,
                           nexting_multipad_render_approval_fn render_approval,
                           nexting_multipad_render_status_fn render_status,
                           void *context) {
  if (adapter == NULL)
    return;
  memset(adapter, 0, sizeof *adapter);
  nexting_device_state_init(&adapter->approval);
  nexting_device_status_init(&adapter->status);
  nexting_device_stream_init(&adapter->stream, adapter->stream_storage,
                             sizeof adapter->stream_storage);
  adapter->now_ms = now_ms;
  adapter->write_frame = write_frame;
  adapter->render_approval = render_approval;
  adapter->render_status = render_status;
  adapter->context = context;
}

bool nexting_multipad_accepts(const nexting_multipad_t *adapter,
                              const uint8_t *bytes, size_t length) {
  if (adapter == NULL || bytes == NULL || length == 0)
    return false;
  if (adapter->frame_active)
    return true;
  for (size_t i = 0; i < length; ++i) {
    if (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\r')
      continue;
    return bytes[i] == '{';
  }
  return false;
}

nexting_device_result_t nexting_multipad_receive(nexting_multipad_t *adapter,
                                                 const uint8_t *bytes,
                                                 size_t length) {
  if (adapter == NULL || (bytes == NULL && length != 0))
    return NEXTING_DEVICE_BAD_MESSAGE;
  adapter->frame_active = true;
  nexting_device_result_t result = nexting_device_stream_push(
      &adapter->stream, bytes, length, handle_message, adapter);
  if (contains_newline(bytes, length))
    adapter->frame_active = false;
  return result;
}

nexting_device_result_t nexting_multipad_choose(
    nexting_multipad_t *adapter, nexting_device_choice_t choice) {
  if (adapter == NULL || adapter->write_frame == NULL)
    return NEXTING_DEVICE_BAD_MESSAGE;
  nexting_device_message_t answer = {0};
  nexting_device_result_t result = nexting_device_state_choose(
      &adapter->approval, choice, adapter_now_ms(adapter), &answer);
  if (result == NEXTING_DEVICE_OK) {
    write_answer(adapter, &answer);
    render_approval(adapter);
  }
  return result;
}

void nexting_multipad_tick(nexting_multipad_t *adapter) {
  if (adapter == NULL)
    return;
  const uint64_t now_ms = adapter_now_ms(adapter);
  if (nexting_device_state_tick(&adapter->approval, now_ms))
    render_approval(adapter);
  nexting_device_message_t answer = {0};
  if (nexting_device_state_retry_answer(&adapter->approval, now_ms, &answer))
    write_answer(adapter, &answer);
}

void nexting_multipad_disconnect(nexting_multipad_t *adapter) {
  if (adapter == NULL)
    return;
  nexting_device_state_disconnect(&adapter->approval);
  nexting_device_status_disconnect(&adapter->status);
  nexting_device_stream_reset(&adapter->stream);
  adapter->frame_active = false;
  render_approval(adapter);
  render_status(adapter);
}
