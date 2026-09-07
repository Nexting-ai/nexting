#ifndef NEXTING_AHAKEY_AUDIO_H
#define NEXTING_AHAKEY_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nexting_device.h"
#include "nexting_device_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NEXTING_AHAKEY_AUDIO_QUEUE_FRAMES 10U
#define NEXTING_AHAKEY_AUDIO_INITIAL_CREDITS 8U
#define NEXTING_AHAKEY_AUDIO_STALL_MS 500U
#define NEXTING_AHAKEY_AUDIO_HOLD_MS 450U
#define NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES 12U
#define NEXTING_AHAKEY_AUDIO_MAX_FRAGMENTS 4U

typedef enum {
  NEXTING_AHAKEY_AUDIO_UNBOUND = 0,
  NEXTING_AHAKEY_AUDIO_UNCONFIGURED,
  NEXTING_AHAKEY_AUDIO_READY,
  NEXTING_AHAKEY_AUDIO_CAPTURING,
  NEXTING_AHAKEY_AUDIO_DRAINING,
  NEXTING_AHAKEY_AUDIO_WAITING_RESULT
} nexting_ahakey_audio_phase_t;

typedef uint64_t (*nexting_ahakey_audio_now_ms_fn)(void *context);
typedef void (*nexting_ahakey_audio_write_control_fn)(
    const nexting_device_message_t *message, void *context);
typedef bool (*nexting_ahakey_audio_write_packet_fn)(const uint8_t *bytes,
                                                     size_t length,
                                                     void *context);
typedef bool (*nexting_ahakey_audio_hw_start_fn)(uint32_t sample_rate,
                                                uint8_t channels,
                                                uint16_t frame_ms,
                                                void *context);
typedef void (*nexting_ahakey_audio_hw_stop_fn)(void *context);
typedef void (*nexting_ahakey_audio_hw_cancel_fn)(void *context);
typedef void (*nexting_ahakey_audio_render_state_fn)(
    nexting_device_audio_state_t state, void *context);

typedef struct {
  nexting_ahakey_audio_phase_t phase;
  uint32_t epoch;
  uint32_t stream_id;
  uint32_t next_stream_nonce;
  uint32_t next_sequence;
  uint32_t sent_frame_count;
  uint32_t last_ack_sequence;
  bool has_ack;
  bool key_down;
  bool latched;
  bool stall_active;
  uint8_t credits;
  uint8_t queue_head;
  uint8_t queue_count;
  uint64_t capture_started_ms;
  uint64_t stall_started_ms;
  nexting_device_audio_end_reason_t drain_reason;
  size_t maximum_packet_bytes;
  uint8_t queued_frames[NEXTING_AHAKEY_AUDIO_QUEUE_FRAMES]
                       [NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  nexting_ahakey_audio_now_ms_fn now_ms;
  nexting_ahakey_audio_write_control_fn write_control;
  nexting_ahakey_audio_write_packet_fn write_packet;
  nexting_ahakey_audio_hw_start_fn hardware_start;
  nexting_ahakey_audio_hw_stop_fn hardware_stop;
  nexting_ahakey_audio_hw_cancel_fn hardware_cancel;
  nexting_ahakey_audio_render_state_fn render_state;
  void *protocol_context;
  void *hardware_context;
} nexting_ahakey_audio_t;

/* Initializes the protocol half. Audio remains unavailable until bind(). */
void nexting_ahakey_audio_init(
    nexting_ahakey_audio_t *audio, nexting_ahakey_audio_now_ms_fn now_ms,
    nexting_ahakey_audio_write_control_fn write_control,
    void *protocol_context);

/*
 * Binds the board port. write_packet receives a complete logical audio packet:
 * either a 192-byte frame or one standard fragment. A compatibility port adds
 * its descriptor opcode (A2 for the supplied X1 descriptor) outside this core.
 */
bool nexting_ahakey_audio_bind(
    nexting_ahakey_audio_t *audio, size_t maximum_packet_bytes,
    nexting_ahakey_audio_write_packet_fn write_packet,
    nexting_ahakey_audio_hw_start_fn hardware_start,
    nexting_ahakey_audio_hw_stop_fn hardware_stop,
    nexting_ahakey_audio_hw_cancel_fn hardware_cancel,
    nexting_ahakey_audio_render_state_fn render_state, void *hardware_context);

/* Handles audio_config, audio_credit, audio_cancel, and audio_state. */
bool nexting_ahakey_audio_on_message(nexting_ahakey_audio_t *audio,
                                    const nexting_device_message_t *message);

/* Physical-key lifecycle. No Host message can call these functions. */
bool nexting_ahakey_audio_voice_pressed(nexting_ahakey_audio_t *audio);
bool nexting_ahakey_audio_voice_released(nexting_ahakey_audio_t *audio);
bool nexting_ahakey_audio_cancel_physical(nexting_ahakey_audio_t *audio);

/* Called by the production microphone driver for each exact 20 ms PCM block. */
bool nexting_ahakey_audio_on_pcm(
    nexting_ahakey_audio_t *audio,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]);
void nexting_ahakey_audio_on_hardware_error(nexting_ahakey_audio_t *audio);

void nexting_ahakey_audio_tick(nexting_ahakey_audio_t *audio);
void nexting_ahakey_audio_disconnect(nexting_ahakey_audio_t *audio);

#ifdef __cplusplus
}
#endif

#endif
