#ifndef NEXTING_AHAKEY_PORT_H
#define NEXTING_AHAKEY_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nexting_ahakey_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NEXTING_AHAKEY_TUNNEL_CONTROL_DOWN 0xA0U
#define NEXTING_AHAKEY_TUNNEL_CONTROL_UP 0xA1U
#define NEXTING_AHAKEY_TUNNEL_AUDIO_UP 0xA2U
#define NEXTING_AHAKEY_TUNNEL_FLOW_DOWN 0xA3U
#define NEXTING_AHAKEY_TUNNEL_MAX_ATT_PAYLOAD 512U

typedef struct {
  uint64_t (*now_ms)(void *context);
  /* Notify raw bytes on the existing encrypted 0x7344 characteristic. */
  bool (*notify_7344)(const uint8_t *bytes, size_t length, void *context);
  /* Preserve stock AA BB ... CC DD traffic on the 0x7343 write path. */
  bool (*vendor_7343_write)(const uint8_t *bytes, size_t length,
                            void *context);
  /* Production PCM driver: 16 kHz, signed 16-bit, mono, exact 320 samples. */
  bool (*microphone_start)(uint32_t sample_rate, uint8_t channels,
                           uint16_t frame_ms, void *context);
  void (*microphone_stop)(void *context);
  void (*microphone_cancel)(void *context);
  nexting_ahakey_render_approval_fn render_approval;
  nexting_ahakey_render_led_fn render_led;
  nexting_ahakey_render_text_fn render_text;
  nexting_ahakey_audio_render_state_fn render_audio_state;
} nexting_ahakey_port_ops_t;

typedef struct {
  nexting_ahakey_t adapter;
  nexting_ahakey_port_ops_t ops;
  const uint8_t *device_info;
  size_t device_info_length;
  size_t att_payload_bytes;
  void *context;
} nexting_ahakey_port_t;

/* device_info storage must remain valid for the life of port. */
bool nexting_ahakey_port_init(nexting_ahakey_port_t *port,
                             const nexting_ahakey_port_ops_t *ops,
                             size_t att_payload_bytes,
                             const uint8_t *device_info,
                             size_t device_info_length, void *context);

/*
 * Call from the 0x7343 write callback. Direct A0/A3 tunnel packets are
 * consumed; stock AA BB vendor frames are forwarded unchanged.
 */
bool nexting_ahakey_port_on_7343_write(nexting_ahakey_port_t *port,
                                      const uint8_t *bytes, size_t length,
                                      bool bonded, bool encrypted);

/* Call after encrypted 0x7344 notifications are enabled. */
bool nexting_ahakey_port_publish_device_info(nexting_ahakey_port_t *port,
                                             bool bonded, bool encrypted);

/* Board ISR/task entry points. PCM must be post-key input, never pre-roll. */
bool nexting_ahakey_port_voice_pressed(nexting_ahakey_port_t *port);
bool nexting_ahakey_port_voice_released(nexting_ahakey_port_t *port);
bool nexting_ahakey_port_voice_cancel(nexting_ahakey_port_t *port);
bool nexting_ahakey_port_on_pcm(
    nexting_ahakey_port_t *port,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]);
void nexting_ahakey_port_on_microphone_error(nexting_ahakey_port_t *port);
void nexting_ahakey_port_tick(nexting_ahakey_port_t *port);
void nexting_ahakey_port_disconnect(nexting_ahakey_port_t *port);

#ifdef __cplusplus
}
#endif

#endif
