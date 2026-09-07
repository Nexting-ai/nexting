#include "nexting_ahakey_port.h"

#include <assert.h>
#include <string.h>

typedef struct {
  uint64_t now_ms;
  uint8_t notifications[64][NEXTING_AHAKEY_TUNNEL_MAX_ATT_PAYLOAD];
  size_t notification_lengths[64];
  size_t notification_count;
  size_t vendor_writes;
  size_t microphone_starts;
} port_test_context_t;

static uint64_t now_ms(void *context) {
  return ((port_test_context_t *)context)->now_ms;
}

static bool notify_7344(const uint8_t *bytes, size_t length, void *context) {
  port_test_context_t *test = (port_test_context_t *)context;
  assert(test->notification_count < 64U);
  assert(length <= NEXTING_AHAKEY_TUNNEL_MAX_ATT_PAYLOAD);
  memcpy(test->notifications[test->notification_count], bytes, length);
  test->notification_lengths[test->notification_count] = length;
  test->notification_count += 1U;
  return true;
}

static bool vendor_write(const uint8_t *bytes, size_t length, void *context) {
  port_test_context_t *test = (port_test_context_t *)context;
  assert(bytes != NULL && length > 0U);
  test->vendor_writes += 1U;
  return true;
}

static bool microphone_start(uint32_t sample_rate, uint8_t channels,
                             uint16_t frame_ms, void *context) {
  port_test_context_t *test = (port_test_context_t *)context;
  assert(sample_rate == 16000U && channels == 1U && frame_ms == 20U);
  test->microphone_starts += 1U;
  return true;
}

static void microphone_noop(void *context) { (void)context; }

static void render_audio_state(nexting_device_audio_state_t state,
                               void *context) {
  (void)state;
  (void)context;
}

static bool tunnel_write(nexting_ahakey_port_t *port, uint8_t opcode,
                         const char *wire) {
  const size_t length = strlen(wire);
  size_t offset = 0U;
  while (offset < length) {
    const size_t remaining = length - offset;
    const size_t chunk_length = remaining < 63U ? remaining : 63U;
    uint8_t packet[64];
    packet[0] = opcode;
    memcpy(packet + 1U, wire + offset, chunk_length);
    if (!nexting_ahakey_port_on_7343_write(
            port, packet, chunk_length + 1U, true, true))
      return false;
    offset += chunk_length;
  }
  return true;
}

int main(void) {
  static const uint8_t info[] =
      "{\"protocol\":\"nexting-device\",\"device_id\":"
      "\"123e4567-e89b-12d3-a456-426614174000\"}";
  port_test_context_t test = {0};
  const nexting_ahakey_port_ops_t ops = {
      now_ms,          notify_7344, vendor_write,    microphone_start,
      microphone_noop, microphone_noop, NULL,       NULL,
      NULL,            render_audio_state,
  };
  nexting_ahakey_port_t port;
  nexting_ahakey_port_t rejected_port;
  static const uint8_t multiline_info[] = "{\n}";
  assert(!nexting_ahakey_port_init(
      &rejected_port, &ops, 64U, multiline_info,
      sizeof multiline_info - 1U, &test));
  assert(nexting_ahakey_port_init(&port, &ops, 64U, info,
                                  sizeof info - 1U, &test));

  /* Enrollment metadata is never exposed before bond + encryption. */
  assert(!nexting_ahakey_port_publish_device_info(&port, false, true));
  assert(test.notification_count == 0U);
  assert(nexting_ahakey_port_publish_device_info(&port, true, true));
  assert(test.notification_count == 3U);
  assert(test.notifications[0][0] == NEXTING_AHAKEY_TUNNEL_CONTROL_UP);
  assert(test.notifications[2][0] == NEXTING_AHAKEY_TUNNEL_CONTROL_UP);
  assert(test.notifications[2][1] == '\n');

  /* Stock AA BB commands stay on the original vendor parser. */
  const uint8_t vendor[] = {0xAAU, 0xBBU, 0x00U, 0xCCU, 0xDDU};
  assert(nexting_ahakey_port_on_7343_write(&port, vendor, sizeof vendor,
                                          false, false));
  assert(test.vendor_writes == 1U);

  static const char config[] =
      "{\"v\":1,\"t\":\"audio_config\",\"epoch\":5,"
      "\"codec\":\"ima_adpcm\",\"sample_rate\":16000,\"channels\":1,"
      "\"frame_ms\":20,\"max_duration_ms\":120000,"
      "\"startup_buffer_ms\":200,\"credits\":8}\n";
  uint8_t rejected[2] = {NEXTING_AHAKEY_TUNNEL_CONTROL_DOWN, '{'};
  assert(!nexting_ahakey_port_on_7343_write(&port, rejected,
                                            sizeof rejected, true, false));
  assert(tunnel_write(&port, NEXTING_AHAKEY_TUNNEL_CONTROL_DOWN, config));
  assert(port.adapter.audio.phase == NEXTING_AHAKEY_AUDIO_READY);

  /* The tunnel config did not start capture; only this physical edge does. */
  assert(test.microphone_starts == 0U);
  assert(nexting_ahakey_port_voice_pressed(&port));
  assert(test.microphone_starts == 1U);
  const size_t after_begin = test.notification_count;
  assert(test.notifications[after_begin - 1U][0] ==
         NEXTING_AHAKEY_TUNNEL_CONTROL_UP);

  int16_t pcm[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME] = {0};
  assert(nexting_ahakey_port_on_pcm(&port, pcm));
  assert(test.notification_count == after_begin + 4U);
  for (size_t i = after_begin; i < test.notification_count; ++i)
    assert(test.notifications[i][0] == NEXTING_AHAKEY_TUNNEL_AUDIO_UP);

  nexting_ahakey_port_disconnect(&port);
  assert(port.adapter.audio.phase == NEXTING_AHAKEY_AUDIO_UNCONFIGURED);
  return 0;
}
