#include "nexting_ahakey_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  uint64_t now_ms;
  char control[512];
  size_t control_length;
  uint8_t packets[128][NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  size_t packet_lengths[128];
  size_t packet_count;
  size_t starts;
  size_t stops;
  size_t cancels;
  nexting_device_audio_state_t rendered_state;
} audio_test_context_t;

static uint64_t now_ms(void *context) {
  return ((audio_test_context_t *)context)->now_ms;
}

static void write_control(const uint8_t *bytes, size_t length, void *context) {
  audio_test_context_t *test = (audio_test_context_t *)context;
  assert(length < sizeof test->control);
  memcpy(test->control, bytes, length);
  test->control[length] = '\0';
  test->control_length = length;
}

static bool write_packet(const uint8_t *bytes, size_t length, void *context) {
  audio_test_context_t *test = (audio_test_context_t *)context;
  assert(test->packet_count < 128U);
  assert(length <= NEXTING_DEVICE_AUDIO_FRAME_BYTES);
  memcpy(test->packets[test->packet_count], bytes, length);
  test->packet_lengths[test->packet_count] = length;
  test->packet_count += 1U;
  return true;
}

static bool hardware_start(uint32_t sample_rate, uint8_t channels,
                           uint16_t frame_ms, void *context) {
  audio_test_context_t *test = (audio_test_context_t *)context;
  assert(sample_rate == 16000U);
  assert(channels == 1U);
  assert(frame_ms == 20U);
  test->starts += 1U;
  return true;
}

static void hardware_stop(void *context) {
  ((audio_test_context_t *)context)->stops += 1U;
}

static void hardware_cancel(void *context) {
  ((audio_test_context_t *)context)->cancels += 1U;
}

static void render_state(nexting_device_audio_state_t state, void *context) {
  ((audio_test_context_t *)context)->rendered_state = state;
}

static void receive(nexting_ahakey_t *adapter, const char *wire) {
  assert(nexting_ahakey_receive(adapter, (const uint8_t *)wire,
                                strlen(wire)) == NEXTING_DEVICE_OK);
}

static void configure(nexting_ahakey_t *adapter, uint32_t epoch) {
  char wire[256];
  const int length = snprintf(
      wire, sizeof wire,
      "{\"v\":1,\"t\":\"audio_config\",\"epoch\":%u,"
      "\"codec\":\"ima_adpcm\",\"sample_rate\":16000,\"channels\":1,"
      "\"frame_ms\":20,\"max_duration_ms\":120000,"
      "\"startup_buffer_ms\":200,\"credits\":8}\n",
      epoch);
  assert(length > 0 && (size_t)length < sizeof wire);
  receive(adapter, wire);
}

static void credit(nexting_ahakey_t *adapter, uint32_t epoch,
                   uint32_t stream_id, uint32_t ack_sequence,
                   uint8_t credits) {
  char wire[192];
  const int length = snprintf(
      wire, sizeof wire,
      "{\"v\":1,\"t\":\"audio_credit\",\"epoch\":%u,"
      "\"stream_id\":%u,\"ack_seq\":%u,\"credits\":%u}\n",
      epoch, stream_id, ack_sequence, (unsigned)credits);
  assert(length > 0 && (size_t)length < sizeof wire);
  receive(adapter, wire);
}

static void result_state(nexting_ahakey_t *adapter, uint32_t epoch,
                         uint32_t stream_id, const char *state) {
  char wire[160];
  const int length = snprintf(
      wire, sizeof wire,
      "{\"v\":1,\"t\":\"audio_state\",\"epoch\":%u,"
      "\"stream_id\":%u,\"state\":\"%s\"}\n",
      epoch, stream_id, state);
  assert(length > 0 && (size_t)length < sizeof wire);
  receive(adapter, wire);
}

int main(void) {
  audio_test_context_t test = {0};
  nexting_ahakey_t adapter;
  nexting_ahakey_init(&adapter, now_ms, write_control, NULL, NULL, NULL, &test);
  assert(nexting_ahakey_bind_device_audio(
      &adapter, 64U, write_packet, hardware_start, hardware_stop,
      hardware_cancel, render_state, &test));

  /* Host readiness configures buffers but cannot start the microphone. */
  configure(&adapter, 77U);
  assert(test.starts == 0U);
  assert(adapter.audio.phase == NEXTING_AHAKEY_AUDIO_READY);
  assert(test.rendered_state == NEXTING_DEVICE_AUDIO_STATE_READY);

  /* Only the physical key starts capture and announces a device stream. */
  test.now_ms = 100U;
  assert(nexting_ahakey_voice_pressed(&adapter));
  assert(test.starts == 1U);
  assert(strstr(test.control, "\"t\":\"audio_begin\"") != NULL);
  assert(test.rendered_state == NEXTING_DEVICE_AUDIO_STATE_RECEIVING);
  const uint32_t first_stream = adapter.audio.stream_id;

  int16_t pcm[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME];
  for (size_t i = 0U; i < NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME; ++i)
    pcm[i] = (int16_t)(i * 3U);

  /* ATT payload 64 fragments each complete 192-byte frame into four packets. */
  assert(nexting_ahakey_on_pcm(&adapter, pcm));
  assert(test.packet_count == 4U);
  assert(test.packets[0][0] == 1U && test.packets[0][1] == 2U);
  assert(test.packets[0][2] == 0U && test.packets[3][2] == 3U);
  assert(test.packets[0][3] == 4U);

  /* Eight credits are bounded; frame nine waits until a new cumulative ACK. */
  for (size_t i = 1U; i < 9U; ++i)
    assert(nexting_ahakey_on_pcm(&adapter, pcm));
  assert(test.packet_count == 32U);
  assert(adapter.audio.queue_count == 1U);
  credit(&adapter, 77U, first_stream, 0U, 1U);
  assert(test.packet_count == 36U);
  assert(adapter.audio.queue_count == 0U);
  /* Replaying the same ACK cannot mint another credit. */
  credit(&adapter, 77U, first_stream, 0U, 1U);
  assert(adapter.audio.credits == 0U);

  /* A short release latches; the next physical press drains and submits. */
  test.now_ms = 200U;
  assert(nexting_ahakey_voice_released(&adapter));
  assert(adapter.audio.phase == NEXTING_AHAKEY_AUDIO_CAPTURING);
  assert(nexting_ahakey_voice_pressed(&adapter));
  assert(test.stops == 1U);
  assert(strstr(test.control, "\"t\":\"audio_end\"") != NULL);
  assert(strstr(test.control, "\"last_seq\":8") != NULL);
  assert(test.rendered_state == NEXTING_DEVICE_AUDIO_STATE_TRANSCRIBING);
  assert(adapter.audio.phase == NEXTING_AHAKEY_AUDIO_WAITING_RESULT);
  assert(!nexting_ahakey_voice_pressed(&adapter));
  result_state(&adapter, 77U, first_stream, "submitted");
  assert(adapter.audio.phase == NEXTING_AHAKEY_AUDIO_READY);
  assert(test.rendered_state == NEXTING_DEVICE_AUDIO_STATE_SUBMITTED);

  /* Ten queued frames are the hard RAM bound; overflow cancels and wipes. */
  test.packet_count = 0U;
  configure(&adapter, 88U);
  test.now_ms = 1000U;
  assert(nexting_ahakey_voice_pressed(&adapter));
  for (size_t i = 0U; i < 18U; ++i)
    assert(nexting_ahakey_on_pcm(&adapter, pcm));
  assert(adapter.audio.queue_count == 10U);
  assert(!nexting_ahakey_on_pcm(&adapter, pcm));
  assert(test.cancels == 1U);
  assert(adapter.audio.queue_count == 0U);
  assert(strstr(test.control, "\"reason\":\"transport\"") != NULL);

  /* A continuous no-credit stall also fails closed by 500 ms. */
  configure(&adapter, 99U);
  test.now_ms = 2000U;
  assert(nexting_ahakey_voice_pressed(&adapter));
  for (size_t i = 0U; i < 9U; ++i)
    assert(nexting_ahakey_on_pcm(&adapter, pcm));
  assert(adapter.audio.queue_count == 1U);
  test.now_ms = 2500U;
  nexting_ahakey_tick(&adapter);
  assert(test.cancels == 2U);
  assert(adapter.audio.queue_count == 0U);

  /* Disconnect erases the epoch and never restores an interrupted stream. */
  configure(&adapter, 100U);
  assert(nexting_ahakey_voice_pressed(&adapter));
  nexting_ahakey_disconnect(&adapter);
  assert(adapter.audio.phase == NEXTING_AHAKEY_AUDIO_UNCONFIGURED);
  assert(adapter.audio.epoch == 0U);
  assert(!nexting_ahakey_voice_pressed(&adapter));

  /* Zero is a valid wire epoch, not an internal unconfigured sentinel. */
  audio_test_context_t zero_epoch_test = {0};
  nexting_ahakey_t zero_epoch_adapter;
  nexting_ahakey_init(&zero_epoch_adapter, now_ms, write_control, NULL, NULL,
                      NULL, &zero_epoch_test);
  assert(nexting_ahakey_bind_device_audio(
      &zero_epoch_adapter, 64U, write_packet, hardware_start, hardware_stop,
      hardware_cancel, render_state, &zero_epoch_test));
  configure(&zero_epoch_adapter, 0U);
  assert(nexting_ahakey_voice_pressed(&zero_epoch_adapter));
  assert(nexting_ahakey_voice_cancel(&zero_epoch_adapter));
  assert(zero_epoch_adapter.audio.phase == NEXTING_AHAKEY_AUDIO_READY);

  return 0;
}
