#include "nexting_device_audio.h"

#include <string.h>

static const int16_t step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,
    17,    19,    21,    23,    25,    28,    31,    34,    37,
    41,    45,    50,    55,    60,    66,    73,    80,    88,
    97,    107,   118,   130,   143,   157,   173,   190,   209,
    230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,
    1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,
    3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
    7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t index_table[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum) {
  if (value < minimum)
    return minimum;
  if (value > maximum)
    return maximum;
  return value;
}

static int16_t apply_code(int16_t predictor, int16_t step, uint8_t code) {
  int32_t difference = step >> 3;
  int32_t next;
  if ((code & 4U) != 0U)
    difference += step;
  if ((code & 2U) != 0U)
    difference += step >> 1;
  if ((code & 1U) != 0U)
    difference += step >> 2;
  next = (int32_t)predictor + (((code & 8U) != 0U) ? -difference : difference);
  return (int16_t)clamp_i32(next, -32768, 32767);
}

static void write_u16_le(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t)(value & 0xffU);
  output[1] = (uint8_t)(value >> 8U);
}

static void write_u32_le(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t)(value & 0xffU);
  output[1] = (uint8_t)((value >> 8U) & 0xffU);
  output[2] = (uint8_t)((value >> 16U) & 0xffU);
  output[3] = (uint8_t)(value >> 24U);
}

static uint16_t read_u16_le(const uint8_t *input) {
  return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8U));
}

static uint32_t read_u32_le(const uint8_t *input) {
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8U) |
         ((uint32_t)input[2] << 16U) | ((uint32_t)input[3] << 24U);
}

uint32_t nexting_device_crc32c(const uint8_t *bytes, size_t length) {
  uint32_t crc = UINT32_MAX;
  if (bytes == NULL && length != 0U)
    return 0U;
  for (size_t index = 0; index < length; ++index) {
    crc ^= bytes[index];
    for (uint8_t bit = 0; bit < 8U; ++bit)
      crc = (crc >> 1U) ^ (((crc & 1U) != 0U) ? UINT32_C(0x82f63b78) : 0U);
  }
  return crc ^ UINT32_MAX;
}

nexting_device_audio_result_t nexting_device_audio_encode_block(
    const int16_t *samples, size_t sample_count, uint8_t step_index,
    int16_t *predictor, uint8_t *payload, size_t payload_capacity) {
  int16_t current_predictor;
  int32_t current_index;
  if (samples == NULL || predictor == NULL || payload == NULL ||
      sample_count != NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME ||
      step_index > 88U)
    return NEXTING_DEVICE_AUDIO_BAD_ARGUMENT;
  if (payload_capacity < NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES)
    return NEXTING_DEVICE_AUDIO_BUFFER_TOO_SMALL;

  memset(payload, 0, NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES);
  *predictor = samples[0];
  current_predictor = samples[0];
  current_index = step_index;
  for (size_t sample_index = 1U; sample_index < sample_count; ++sample_index) {
    const int16_t step = step_table[current_index];
    int32_t difference = (int32_t)samples[sample_index] - current_predictor;
    int32_t threshold = step;
    uint8_t code = 0U;
    const size_t code_index = sample_index - 1U;
    const size_t byte_index = code_index >> 1U;
    if (difference < 0) {
      code = 8U;
      difference = -difference;
    }
    if (difference >= threshold) {
      code |= 4U;
      difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold) {
      code |= 2U;
      difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold)
      code |= 1U;

    current_predictor = apply_code(current_predictor, step, code);
    current_index = clamp_i32(current_index + index_table[code & 7U], 0, 88);
    if ((code_index & 1U) == 0U)
      payload[byte_index] = code;
    else
      payload[byte_index] |= (uint8_t)(code << 4U);
  }
  return NEXTING_DEVICE_AUDIO_OK;
}

nexting_device_audio_result_t nexting_device_audio_decode_block(
    int16_t predictor, uint8_t step_index, const uint8_t *payload,
    size_t payload_length, size_t sample_count, int16_t *samples,
    size_t sample_capacity) {
  int16_t current_predictor = predictor;
  int32_t current_index = step_index;
  if (payload == NULL || samples == NULL || sample_count < 1U ||
      sample_count > NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME ||
      step_index > 88U)
    return NEXTING_DEVICE_AUDIO_BAD_ARGUMENT;
  const size_t expected_payload = sample_count / 2U;
  if (payload_length != expected_payload)
    return NEXTING_DEVICE_AUDIO_BAD_ARGUMENT;
  if (sample_capacity < sample_count)
    return NEXTING_DEVICE_AUDIO_BUFFER_TOO_SMALL;
  if (((sample_count - 1U) & 1U) != 0U &&
      (payload[payload_length - 1U] & 0xf0U) != 0U)
    return NEXTING_DEVICE_AUDIO_BAD_FRAME;

  samples[0] = predictor;
  for (size_t sample_index = 1U; sample_index < sample_count; ++sample_index) {
    const size_t code_index = sample_index - 1U;
    const uint8_t packed = payload[code_index >> 1U];
    const uint8_t code = ((code_index & 1U) == 0U)
                             ? (uint8_t)(packed & 0x0fU)
                             : (uint8_t)(packed >> 4U);
    current_predictor =
        apply_code(current_predictor, step_table[current_index], code);
    current_index = clamp_i32(current_index + index_table[code & 7U], 0, 88);
    samples[sample_index] = current_predictor;
  }
  return NEXTING_DEVICE_AUDIO_OK;
}

nexting_device_audio_result_t nexting_device_audio_encode_frame(
    uint32_t epoch, uint32_t stream_id, uint32_t sequence,
    uint32_t sample_index, const int16_t *samples, size_t sample_count,
    uint8_t step_index, uint8_t *output, size_t output_capacity,
    size_t *output_length) {
  int16_t predictor;
  nexting_device_audio_result_t result;
  if (samples == NULL || output == NULL || output_length == NULL ||
      sample_count != NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME ||
      step_index > 88U || sequence >= NEXTING_DEVICE_AUDIO_MAX_FRAMES ||
      sample_index != sequence * NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME)
    return NEXTING_DEVICE_AUDIO_BAD_ARGUMENT;
  if (output_capacity < NEXTING_DEVICE_AUDIO_FRAME_BYTES)
    return NEXTING_DEVICE_AUDIO_BUFFER_TOO_SMALL;

  memset(output, 0, NEXTING_DEVICE_AUDIO_FRAME_BYTES);
  output[0] = NEXTING_DEVICE_AUDIO_WIRE_VERSION;
  output[1] = 1U;
  output[2] = NEXTING_DEVICE_AUDIO_CODEC_IMA_ADPCM;
  write_u32_le(output + 4U, epoch);
  write_u32_le(output + 8U, stream_id);
  write_u32_le(output + 12U, sequence);
  write_u32_le(output + 16U, sample_index);
  write_u16_le(output + 20U, (uint16_t)sample_count);
  output[24] = step_index;
  write_u16_le(output + 26U, NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES);
  result = nexting_device_audio_encode_block(
      samples, sample_count, step_index, &predictor,
      output + NEXTING_DEVICE_AUDIO_HEADER_BYTES,
      output_capacity - NEXTING_DEVICE_AUDIO_HEADER_BYTES);
  if (result != NEXTING_DEVICE_AUDIO_OK)
    return result;
  write_u16_le(output + 22U, (uint16_t)predictor);
  write_u32_le(output + NEXTING_DEVICE_AUDIO_FRAME_BYTES - 4U,
               nexting_device_crc32c(output,
                                     NEXTING_DEVICE_AUDIO_FRAME_BYTES - 4U));
  *output_length = NEXTING_DEVICE_AUDIO_FRAME_BYTES;
  return NEXTING_DEVICE_AUDIO_OK;
}

nexting_device_audio_result_t nexting_device_audio_decode_frame(
    const uint8_t *wire, size_t wire_length,
    nexting_device_audio_frame_t *output) {
  nexting_device_audio_result_t result;
  if (wire == NULL || output == NULL)
    return NEXTING_DEVICE_AUDIO_BAD_ARGUMENT;
  if (wire_length != NEXTING_DEVICE_AUDIO_FRAME_BYTES ||
      wire[0] != NEXTING_DEVICE_AUDIO_WIRE_VERSION || wire[1] != 1U ||
      wire[2] != NEXTING_DEVICE_AUDIO_CODEC_IMA_ADPCM || wire[3] != 0U ||
      read_u16_le(wire + 20U) != NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME ||
      wire[24] > 88U || wire[25] != 0U ||
      read_u16_le(wire + 26U) != NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES ||
      read_u32_le(wire + 12U) >= NEXTING_DEVICE_AUDIO_MAX_FRAMES ||
      read_u32_le(wire + 16U) !=
          read_u32_le(wire + 12U) * NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME ||
      (wire[NEXTING_DEVICE_AUDIO_FRAME_BYTES - 5U] & 0xf0U) != 0U ||
      read_u32_le(wire + NEXTING_DEVICE_AUDIO_FRAME_BYTES - 4U) !=
          nexting_device_crc32c(wire,
                               NEXTING_DEVICE_AUDIO_FRAME_BYTES - 4U))
    return NEXTING_DEVICE_AUDIO_BAD_FRAME;

  memset(output, 0, sizeof(*output));
  output->epoch = read_u32_le(wire + 4U);
  output->stream_id = read_u32_le(wire + 8U);
  output->sequence = read_u32_le(wire + 12U);
  output->sample_index = read_u32_le(wire + 16U);
  output->sample_count = read_u16_le(wire + 20U);
  {
    const uint16_t raw_predictor = read_u16_le(wire + 22U);
    const int32_t signed_predictor =
        raw_predictor <= INT16_MAX ? (int32_t)raw_predictor
                                   : (int32_t)raw_predictor - 65536;
    output->predictor = (int16_t)signed_predictor;
  }
  output->step_index = wire[24];
  result = nexting_device_audio_decode_block(
      output->predictor, output->step_index,
      wire + NEXTING_DEVICE_AUDIO_HEADER_BYTES,
      NEXTING_DEVICE_AUDIO_PAYLOAD_BYTES, output->sample_count, output->samples,
      NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME);
  return result == NEXTING_DEVICE_AUDIO_OK ? NEXTING_DEVICE_AUDIO_OK
                                           : NEXTING_DEVICE_AUDIO_BAD_FRAME;
}
