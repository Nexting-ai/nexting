#ifndef NEXTING_DEVICE_AUDIO_H
#define NEXTING_DEVICE_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXTING_DEVICE_AUDIO_WIRE_VERSION 1U
#define NEXTING_DEVICE_AUDIO_CODEC_IMA_ADPCM 1U
#define NEXTING_DEVICE_AUDIO_SAMPLE_RATE 16000U
#define NEXTING_DEVICE_AUDIO_FRAME_MS 20U
#define NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME 320U
#define NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES 160U
#define NEXTING_DEVICE_AUDIO_HEADER_BYTES 28U
#define NEXTING_DEVICE_AUDIO_FRAME_BYTES 192U
#define NEXTING_DEVICE_AUDIO_MAX_FRAMES 6000U
#define NEXTING_DEVICE_AUDIO_MAX_SAMPLES 1920000U

typedef enum {
  NEXTING_DEVICE_AUDIO_OK = 0,
  NEXTING_DEVICE_AUDIO_BAD_ARGUMENT,
  NEXTING_DEVICE_AUDIO_BUFFER_TOO_SMALL,
  NEXTING_DEVICE_AUDIO_BAD_FRAME
} nexting_device_audio_result_t;

typedef struct {
  uint32_t epoch;
  uint32_t stream_id;
  uint32_t sequence;
  uint32_t sample_index;
  uint16_t sample_count;
  int16_t predictor;
  uint8_t step_index;
  int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME];
} nexting_device_audio_frame_t;

uint32_t nexting_device_crc32c(const uint8_t *bytes, size_t length);

nexting_device_audio_result_t nexting_device_audio_encode_block(
    const int16_t *samples, size_t sample_count, uint8_t step_index,
    int16_t *predictor, uint8_t *payload, size_t payload_capacity);

nexting_device_audio_result_t nexting_device_audio_decode_block(
    int16_t predictor, uint8_t step_index, const uint8_t *payload,
    size_t payload_length, size_t sample_count, int16_t *samples,
    size_t sample_capacity);

nexting_device_audio_result_t nexting_device_audio_encode_frame(
    uint32_t epoch, uint32_t stream_id, uint32_t sequence,
    uint32_t sample_index, const int16_t *samples, size_t sample_count,
    uint8_t step_index, uint8_t *output, size_t output_capacity,
    size_t *output_length);

nexting_device_audio_result_t nexting_device_audio_decode_frame(
    const uint8_t *wire, size_t wire_length,
    nexting_device_audio_frame_t *output);

#ifdef __cplusplus
}
#endif

#endif
