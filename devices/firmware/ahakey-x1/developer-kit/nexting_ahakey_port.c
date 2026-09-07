#include "nexting_ahakey_port.h"

#include <string.h>

static uint64_t port_now_ms(void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  return port->ops.now_ms(port->context);
}

static bool notify_logical(nexting_ahakey_port_t *port, uint8_t opcode,
                           const uint8_t *bytes, size_t length) {
  if (port == NULL || bytes == NULL || length == 0U ||
      port->ops.notify_7344 == NULL || port->att_payload_bytes <= 1U)
    return false;
  const size_t chunk_capacity = port->att_payload_bytes - 1U;
  uint8_t packet[NEXTING_AHAKEY_TUNNEL_MAX_ATT_PAYLOAD];
  size_t offset = 0U;
  while (offset < length) {
    const size_t remaining = length - offset;
    const size_t chunk_length =
        remaining < chunk_capacity ? remaining : chunk_capacity;
    packet[0] = opcode;
    memcpy(packet + 1U, bytes + offset, chunk_length);
    if (!port->ops.notify_7344(packet, chunk_length + 1U, port->context)) {
      memset(packet, 0, sizeof packet);
      return false;
    }
    offset += chunk_length;
  }
  memset(packet, 0, sizeof packet);
  return true;
}

static void write_control(const uint8_t *bytes, size_t length, void *context) {
  (void)notify_logical((nexting_ahakey_port_t *)context,
                       NEXTING_AHAKEY_TUNNEL_CONTROL_UP, bytes, length);
}

static bool write_audio(const uint8_t *bytes, size_t length, void *context) {
  return notify_logical((nexting_ahakey_port_t *)context,
                        NEXTING_AHAKEY_TUNNEL_AUDIO_UP, bytes, length);
}

static bool microphone_start(uint32_t sample_rate, uint8_t channels,
                             uint16_t frame_ms, void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  return port->ops.microphone_start(sample_rate, channels, frame_ms,
                                    port->context);
}

static void microphone_stop(void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  port->ops.microphone_stop(port->context);
}

static void microphone_cancel(void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  port->ops.microphone_cancel(port->context);
}

static void render_audio_state(nexting_device_audio_state_t state,
                               void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  if (port->ops.render_audio_state != NULL)
    port->ops.render_audio_state(state, port->context);
}

static void render_approval(const nexting_device_state_t *state,
                            void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  if (port->ops.render_approval != NULL)
    port->ops.render_approval(state, port->context);
}

static void render_led(ahakey_state_t state, void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  if (port->ops.render_led != NULL)
    port->ops.render_led(state, port->context);
}

static void render_text(const nexting_device_text_payload_t *text,
                        void *context) {
  nexting_ahakey_port_t *port = (nexting_ahakey_port_t *)context;
  if (port->ops.render_text != NULL)
    port->ops.render_text(text, port->context);
}

bool nexting_ahakey_port_init(nexting_ahakey_port_t *port,
                             const nexting_ahakey_port_ops_t *ops,
                             size_t att_payload_bytes,
                             const uint8_t *device_info,
                             size_t device_info_length, void *context) {
  if (port == NULL || ops == NULL || ops->now_ms == NULL ||
      ops->notify_7344 == NULL || ops->vendor_7343_write == NULL ||
      ops->microphone_start == NULL || ops->microphone_stop == NULL ||
      ops->microphone_cancel == NULL || ops->render_audio_state == NULL ||
      device_info == NULL ||
      device_info_length == 0U ||
      device_info_length > NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES ||
      memchr(device_info, '\n', device_info_length) != NULL ||
      memchr(device_info, '\r', device_info_length) != NULL ||
      att_payload_bytes < 64U ||
      att_payload_bytes > NEXTING_AHAKEY_TUNNEL_MAX_ATT_PAYLOAD)
    return false;
  memset(port, 0, sizeof *port);
  port->ops = *ops;
  port->device_info = device_info;
  port->device_info_length = device_info_length;
  port->att_payload_bytes = att_payload_bytes;
  port->context = context;
  nexting_ahakey_init(&port->adapter, port_now_ms, write_control,
                      render_approval, render_led, render_text, port);
  return nexting_ahakey_bind_device_audio(
      &port->adapter, att_payload_bytes - 1U, write_audio, microphone_start,
      microphone_stop, microphone_cancel, render_audio_state, port);
}

bool nexting_ahakey_port_on_7343_write(nexting_ahakey_port_t *port,
                                      const uint8_t *bytes, size_t length,
                                      bool bonded, bool encrypted) {
  if (port == NULL || bytes == NULL || length == 0U)
    return false;
  const uint8_t opcode = bytes[0];
  if (opcode != NEXTING_AHAKEY_TUNNEL_CONTROL_DOWN &&
      opcode != NEXTING_AHAKEY_TUNNEL_FLOW_DOWN)
    return port->ops.vendor_7343_write(bytes, length, port->context);
  if (!bonded || !encrypted || length == 1U)
    return false;
  return nexting_ahakey_receive(&port->adapter, bytes + 1U, length - 1U) ==
         NEXTING_DEVICE_OK;
}

bool nexting_ahakey_port_publish_device_info(nexting_ahakey_port_t *port,
                                             bool bonded, bool encrypted) {
  if (port == NULL || !bonded || !encrypted)
    return false;
  if (!notify_logical(port, NEXTING_AHAKEY_TUNNEL_CONTROL_UP,
                      port->device_info, port->device_info_length))
    return false;
  static const uint8_t newline = '\n';
  return notify_logical(port, NEXTING_AHAKEY_TUNNEL_CONTROL_UP, &newline, 1U);
}

bool nexting_ahakey_port_voice_pressed(nexting_ahakey_port_t *port) {
  return port != NULL && nexting_ahakey_voice_pressed(&port->adapter);
}

bool nexting_ahakey_port_voice_released(nexting_ahakey_port_t *port) {
  return port != NULL && nexting_ahakey_voice_released(&port->adapter);
}

bool nexting_ahakey_port_voice_cancel(nexting_ahakey_port_t *port) {
  return port != NULL && nexting_ahakey_voice_cancel(&port->adapter);
}

bool nexting_ahakey_port_on_pcm(
    nexting_ahakey_port_t *port,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]) {
  return port != NULL && nexting_ahakey_on_pcm(&port->adapter, samples);
}

void nexting_ahakey_port_on_microphone_error(nexting_ahakey_port_t *port) {
  if (port != NULL)
    nexting_ahakey_on_audio_hardware_error(&port->adapter);
}

void nexting_ahakey_port_tick(nexting_ahakey_port_t *port) {
  if (port != NULL)
    nexting_ahakey_tick(&port->adapter);
}

void nexting_ahakey_port_disconnect(nexting_ahakey_port_t *port) {
  if (port != NULL)
    nexting_ahakey_disconnect(&port->adapter);
}
