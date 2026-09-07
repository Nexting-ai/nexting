#include "nexting_ahakey_audio.h"

#include <string.h>

static uint64_t audio_now_ms(const nexting_ahakey_audio_t *audio) {
  if (audio == NULL || audio->now_ms == NULL)
    return 0U;
  return audio->now_ms(audio->protocol_context);
}

static void render(nexting_ahakey_audio_t *audio,
                   nexting_device_audio_state_t state) {
  if (audio != NULL && audio->render_state != NULL)
    audio->render_state(state, audio->hardware_context);
}

static void emit(nexting_ahakey_audio_t *audio,
                 const nexting_device_message_t *message) {
  if (audio != NULL && audio->write_control != NULL)
    audio->write_control(message, audio->protocol_context);
}

static void wipe_queue(nexting_ahakey_audio_t *audio) {
  if (audio == NULL)
    return;
  memset(audio->queued_frames, 0, sizeof audio->queued_frames);
  audio->queue_head = 0U;
  audio->queue_count = 0U;
  audio->stall_active = false;
  audio->stall_started_ms = 0U;
}

static void reset_stream(nexting_ahakey_audio_t *audio,
                         nexting_ahakey_audio_phase_t next_phase) {
  wipe_queue(audio);
  audio->phase = next_phase;
  audio->next_sequence = 0U;
  audio->sent_frame_count = 0U;
  audio->last_ack_sequence = 0U;
  audio->has_ack = false;
  audio->key_down = false;
  audio->latched = false;
  audio->capture_started_ms = 0U;
  audio->drain_reason = NEXTING_DEVICE_AUDIO_END_NONE;
}

static void emit_cancel(nexting_ahakey_audio_t *audio,
                        nexting_device_audio_cancel_reason_t reason) {
  nexting_device_message_t message = {0};
  message.type = NEXTING_DEVICE_MESSAGE_AUDIO_CANCEL;
  message.interaction.audio.epoch = audio->epoch;
  message.interaction.audio.has_stream_id = true;
  message.interaction.audio.stream_id = audio->stream_id;
  message.interaction.audio.cancel_reason = reason;
  emit(audio, &message);
}

static void cancel_stream(nexting_ahakey_audio_t *audio,
                          nexting_device_audio_cancel_reason_t reason,
                          bool notify_host) {
  if (audio == NULL)
    return;
  const bool was_active = audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING ||
                          audio->phase == NEXTING_AHAKEY_AUDIO_DRAINING;
  if (was_active)
    audio->phase = NEXTING_AHAKEY_AUDIO_DRAINING;
  if (was_active && audio->hardware_cancel != NULL)
    audio->hardware_cancel(audio->hardware_context);
  if (notify_host && was_active)
    emit_cancel(audio, reason);
  const nexting_ahakey_audio_phase_t next_phase =
      audio->phase == NEXTING_AHAKEY_AUDIO_UNBOUND
          ? NEXTING_AHAKEY_AUDIO_UNBOUND
          : (audio->phase == NEXTING_AHAKEY_AUDIO_UNCONFIGURED
                 ? NEXTING_AHAKEY_AUDIO_UNCONFIGURED
                 : NEXTING_AHAKEY_AUDIO_READY);
  reset_stream(audio, next_phase);
  if (next_phase == NEXTING_AHAKEY_AUDIO_READY)
    render(audio, NEXTING_DEVICE_AUDIO_STATE_READY);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value) {
  bytes[offset] = (uint8_t)value;
  bytes[offset + 1U] = (uint8_t)(value >> 8U);
  bytes[offset + 2U] = (uint8_t)(value >> 16U);
  bytes[offset + 3U] = (uint8_t)(value >> 24U);
}

static bool send_frame(nexting_ahakey_audio_t *audio, const uint8_t *frame) {
  if (audio == NULL || frame == NULL || audio->write_packet == NULL)
    return false;
  if (audio->maximum_packet_bytes >= NEXTING_DEVICE_AUDIO_FRAME_BYTES)
    return audio->write_packet(frame, NEXTING_DEVICE_AUDIO_FRAME_BYTES,
                               audio->hardware_context);
  if (audio->maximum_packet_bytes <= NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES)
    return false;
  const size_t payload_bytes =
      audio->maximum_packet_bytes - NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES;
  const size_t fragment_count =
      (NEXTING_DEVICE_AUDIO_FRAME_BYTES + payload_bytes - 1U) / payload_bytes;
  if (fragment_count == 0U ||
      fragment_count > NEXTING_AHAKEY_AUDIO_MAX_FRAGMENTS)
    return false;

  uint8_t packet[NEXTING_DEVICE_AUDIO_FRAME_BYTES];
  for (size_t index = 0U; index < fragment_count; ++index) {
    const size_t lower = index * payload_bytes;
    const size_t remaining = NEXTING_DEVICE_AUDIO_FRAME_BYTES - lower;
    const size_t length = remaining < payload_bytes ? remaining : payload_bytes;
    packet[0] = NEXTING_DEVICE_AUDIO_WIRE_VERSION;
    packet[1] = 2U;
    packet[2] = (uint8_t)index;
    packet[3] = (uint8_t)fragment_count;
    write_u32(packet, 4U, audio->stream_id);
    write_u32(packet, 8U, audio->sent_frame_count);
    memcpy(packet + NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES, frame + lower,
           length);
    if (!audio->write_packet(
            packet, NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES + length,
            audio->hardware_context))
      return false;
  }
  memset(packet, 0, sizeof packet);
  return true;
}

static void finish_end(nexting_ahakey_audio_t *audio) {
  if (audio == NULL || audio->next_sequence == 0U)
    return;
  nexting_device_message_t message = {0};
  message.type = NEXTING_DEVICE_MESSAGE_AUDIO_END;
  message.interaction.audio.epoch = audio->epoch;
  message.interaction.audio.has_stream_id = true;
  message.interaction.audio.stream_id = audio->stream_id;
  message.interaction.audio.last_sequence = audio->next_sequence - 1U;
  message.interaction.audio.sample_count =
      audio->next_sequence * NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME;
  message.interaction.audio.end_reason = audio->drain_reason;
  emit(audio, &message);
  reset_stream(audio, NEXTING_AHAKEY_AUDIO_WAITING_RESULT);
  render(audio, NEXTING_DEVICE_AUDIO_STATE_TRANSCRIBING);
}

static bool flush_queue(nexting_ahakey_audio_t *audio) {
  if (audio == NULL)
    return false;
  while (audio->credits > 0U && audio->queue_count > 0U) {
    uint8_t *frame = audio->queued_frames[audio->queue_head];
    if (!send_frame(audio, frame)) {
      cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_TRANSPORT, true);
      return false;
    }
    memset(frame, 0, NEXTING_DEVICE_AUDIO_FRAME_BYTES);
    audio->queue_head =
        (uint8_t)((audio->queue_head + 1U) % NEXTING_AHAKEY_AUDIO_QUEUE_FRAMES);
    audio->queue_count -= 1U;
    audio->credits -= 1U;
    audio->sent_frame_count += 1U;
  }
  if (audio->queue_count == 0U) {
    audio->stall_active = false;
    audio->stall_started_ms = 0U;
    if (audio->phase == NEXTING_AHAKEY_AUDIO_DRAINING)
      finish_end(audio);
  } else if (audio->credits == 0U && !audio->stall_active) {
    audio->stall_active = true;
    audio->stall_started_ms = audio_now_ms(audio);
  }
  return true;
}

static bool begin_stream(nexting_ahakey_audio_t *audio) {
  if (audio == NULL || audio->phase != NEXTING_AHAKEY_AUDIO_READY ||
      audio->hardware_start == NULL)
    return false;
  reset_stream(audio, NEXTING_AHAKEY_AUDIO_READY);
  audio->credits = NEXTING_AHAKEY_AUDIO_INITIAL_CREDITS;
  audio->next_stream_nonce += 1U;
  audio->stream_id = audio->epoch ^ (uint32_t)audio_now_ms(audio) ^
                     (audio->next_stream_nonce * 0x9E3779B9U);
  if (audio->stream_id == 0U)
    audio->stream_id = audio->next_stream_nonce;
  if (!audio->hardware_start(NEXTING_DEVICE_AUDIO_SAMPLE_RATE, 1U,
                             NEXTING_DEVICE_AUDIO_FRAME_MS,
                             audio->hardware_context)) {
    render(audio, NEXTING_DEVICE_AUDIO_STATE_ERROR);
    return false;
  }
  audio->phase = NEXTING_AHAKEY_AUDIO_CAPTURING;
  audio->key_down = true;
  audio->capture_started_ms = audio_now_ms(audio);

  nexting_device_message_t message = {0};
  message.type = NEXTING_DEVICE_MESSAGE_AUDIO_BEGIN;
  message.interaction.audio.epoch = audio->epoch;
  message.interaction.audio.has_stream_id = true;
  message.interaction.audio.stream_id = audio->stream_id;
  message.interaction.audio.codec =
      NEXTING_DEVICE_AUDIO_CONTROL_CODEC_IMA_ADPCM;
  message.interaction.audio.sample_rate = NEXTING_DEVICE_AUDIO_SAMPLE_RATE;
  message.interaction.audio.channels = 1U;
  message.interaction.audio.frame_ms = NEXTING_DEVICE_AUDIO_FRAME_MS;
  emit(audio, &message);
  render(audio, NEXTING_DEVICE_AUDIO_STATE_RECEIVING);
  return true;
}

static bool stop_stream(nexting_ahakey_audio_t *audio,
                        nexting_device_audio_end_reason_t reason) {
  if (audio == NULL || audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING)
    return false;
  audio->phase = NEXTING_AHAKEY_AUDIO_DRAINING;
  audio->key_down = false;
  audio->latched = false;
  audio->drain_reason = reason;
  if (audio->hardware_stop != NULL)
    audio->hardware_stop(audio->hardware_context);
  if (audio->next_sequence == 0U) {
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_LOCAL, true);
    return false;
  }
  return flush_queue(audio);
}

void nexting_ahakey_audio_init(
    nexting_ahakey_audio_t *audio, nexting_ahakey_audio_now_ms_fn now_ms,
    nexting_ahakey_audio_write_control_fn write_control,
    void *protocol_context) {
  if (audio == NULL)
    return;
  memset(audio, 0, sizeof *audio);
  audio->phase = NEXTING_AHAKEY_AUDIO_UNBOUND;
  audio->now_ms = now_ms;
  audio->write_control = write_control;
  audio->protocol_context = protocol_context;
}

bool nexting_ahakey_audio_bind(
    nexting_ahakey_audio_t *audio, size_t maximum_packet_bytes,
    nexting_ahakey_audio_write_packet_fn write_packet,
    nexting_ahakey_audio_hw_start_fn hardware_start,
    nexting_ahakey_audio_hw_stop_fn hardware_stop,
    nexting_ahakey_audio_hw_cancel_fn hardware_cancel,
    nexting_ahakey_audio_render_state_fn render_state, void *hardware_context) {
  if (audio == NULL || write_packet == NULL || hardware_start == NULL ||
      hardware_stop == NULL || hardware_cancel == NULL ||
      maximum_packet_bytes < 60U || maximum_packet_bytes > 511U)
    return false;
  const size_t fragment_payload =
      maximum_packet_bytes - NEXTING_AHAKEY_AUDIO_FRAGMENT_HEADER_BYTES;
  if (maximum_packet_bytes < NEXTING_DEVICE_AUDIO_FRAME_BYTES &&
      (NEXTING_DEVICE_AUDIO_FRAME_BYTES + fragment_payload - 1U) /
              fragment_payload >
          NEXTING_AHAKEY_AUDIO_MAX_FRAGMENTS)
    return false;
  audio->maximum_packet_bytes = maximum_packet_bytes;
  audio->write_packet = write_packet;
  audio->hardware_start = hardware_start;
  audio->hardware_stop = hardware_stop;
  audio->hardware_cancel = hardware_cancel;
  audio->render_state = render_state;
  audio->hardware_context = hardware_context;
  reset_stream(audio, NEXTING_AHAKEY_AUDIO_UNCONFIGURED);
  return true;
}

bool nexting_ahakey_audio_on_message(nexting_ahakey_audio_t *audio,
                                    const nexting_device_message_t *message) {
  if (audio == NULL || message == NULL)
    return false;
  if (message->type == NEXTING_DEVICE_MESSAGE_AUDIO_CONFIG) {
    if (audio->phase == NEXTING_AHAKEY_AUDIO_UNBOUND)
      return false;
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_HOST, false);
    audio->epoch = message->interaction.audio.epoch;
    audio->credits = message->interaction.audio.credits;
    reset_stream(audio, NEXTING_AHAKEY_AUDIO_READY);
    render(audio, NEXTING_DEVICE_AUDIO_STATE_READY);
    return true;
  }
  if (audio->phase == NEXTING_AHAKEY_AUDIO_UNBOUND ||
      audio->phase == NEXTING_AHAKEY_AUDIO_UNCONFIGURED ||
      message->interaction.audio.epoch != audio->epoch)
    return false;
  if (message->type == NEXTING_DEVICE_MESSAGE_AUDIO_CREDIT) {
    if ((audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING &&
         audio->phase != NEXTING_AHAKEY_AUDIO_DRAINING) ||
        !message->interaction.audio.has_stream_id ||
        message->interaction.audio.stream_id != audio->stream_id ||
        message->interaction.audio.ack_sequence >= audio->sent_frame_count)
      return false;
    if (audio->has_ack &&
        message->interaction.audio.ack_sequence <= audio->last_ack_sequence)
      return true;
    audio->has_ack = true;
    audio->last_ack_sequence = message->interaction.audio.ack_sequence;
    const unsigned restored =
        (unsigned)audio->credits + message->interaction.audio.credits;
    audio->credits = (uint8_t)(restored > 32U ? 32U : restored);
    audio->stall_active = false;
    audio->stall_started_ms = 0U;
    return flush_queue(audio);
  }
  if (message->type == NEXTING_DEVICE_MESSAGE_AUDIO_CANCEL) {
    if (!message->interaction.audio.has_stream_id ||
        message->interaction.audio.stream_id != audio->stream_id)
      return false;
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_HOST, false);
    return true;
  }
  if (message->type == NEXTING_DEVICE_MESSAGE_AUDIO_STATE) {
    if (message->interaction.audio.state == NEXTING_DEVICE_AUDIO_STATE_READY) {
      if (audio->phase != NEXTING_AHAKEY_AUDIO_READY)
        return false;
      render(audio, NEXTING_DEVICE_AUDIO_STATE_READY);
      return true;
    }
    if (!message->interaction.audio.has_stream_id ||
        message->interaction.audio.stream_id != audio->stream_id)
      return false;
    if (message->interaction.audio.state ==
        NEXTING_DEVICE_AUDIO_STATE_RECEIVING) {
      if (audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING)
        return false;
      render(audio, NEXTING_DEVICE_AUDIO_STATE_RECEIVING);
      return true;
    }
    if (message->interaction.audio.state ==
        NEXTING_DEVICE_AUDIO_STATE_TRANSCRIBING) {
      if (audio->phase != NEXTING_AHAKEY_AUDIO_WAITING_RESULT)
        return false;
      render(audio, NEXTING_DEVICE_AUDIO_STATE_TRANSCRIBING);
      return true;
    }
    if (message->interaction.audio.state !=
            NEXTING_DEVICE_AUDIO_STATE_SUBMITTED &&
        message->interaction.audio.state != NEXTING_DEVICE_AUDIO_STATE_ERROR)
      return false;
    if (audio->phase != NEXTING_AHAKEY_AUDIO_WAITING_RESULT)
      return false;
    render(audio, message->interaction.audio.state);
    reset_stream(audio, NEXTING_AHAKEY_AUDIO_READY);
    return true;
  }
  return false;
}

bool nexting_ahakey_audio_voice_pressed(nexting_ahakey_audio_t *audio) {
  if (audio == NULL)
    return false;
  if (audio->phase == NEXTING_AHAKEY_AUDIO_READY)
    return begin_stream(audio);
  if (audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING && audio->latched &&
      !audio->key_down)
    return stop_stream(audio, NEXTING_DEVICE_AUDIO_END_SUBMITTED);
  return false;
}

bool nexting_ahakey_audio_voice_released(nexting_ahakey_audio_t *audio) {
  if (audio == NULL || audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING ||
      !audio->key_down)
    return false;
  audio->key_down = false;
  if (audio_now_ms(audio) - audio->capture_started_ms <
      NEXTING_AHAKEY_AUDIO_HOLD_MS) {
    audio->latched = true;
    return true;
  }
  return stop_stream(audio, NEXTING_DEVICE_AUDIO_END_SUBMITTED);
}

bool nexting_ahakey_audio_cancel_physical(nexting_ahakey_audio_t *audio) {
  if (audio == NULL || (audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING &&
                        audio->phase != NEXTING_AHAKEY_AUDIO_DRAINING))
    return false;
  cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_LOCAL, true);
  return true;
}

bool nexting_ahakey_audio_on_pcm(
    nexting_ahakey_audio_t *audio,
    const int16_t samples[NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME]) {
  if (audio == NULL || samples == NULL ||
      audio->phase != NEXTING_AHAKEY_AUDIO_CAPTURING)
    return false;
  if (audio->next_sequence >= NEXTING_DEVICE_AUDIO_MAX_FRAMES) {
    (void)stop_stream(audio, NEXTING_DEVICE_AUDIO_END_MAX_DURATION);
    return false;
  }
  if (audio->queue_count >= NEXTING_AHAKEY_AUDIO_QUEUE_FRAMES) {
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_TRANSPORT, true);
    return false;
  }
  const uint8_t tail = (uint8_t)((audio->queue_head + audio->queue_count) %
                                 NEXTING_AHAKEY_AUDIO_QUEUE_FRAMES);
  size_t encoded_length = 0U;
  if (nexting_device_audio_encode_frame(
          audio->epoch, audio->stream_id, audio->next_sequence,
          audio->next_sequence * NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME,
          samples, NEXTING_DEVICE_AUDIO_SAMPLES_PER_FRAME, 0U,
          audio->queued_frames[tail], NEXTING_DEVICE_AUDIO_FRAME_BYTES,
          &encoded_length) != NEXTING_DEVICE_AUDIO_OK ||
      encoded_length != NEXTING_DEVICE_AUDIO_FRAME_BYTES) {
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_HARDWARE, true);
    return false;
  }
  audio->queue_count += 1U;
  audio->next_sequence += 1U;
  if (!flush_queue(audio))
    return false;
  if (audio->next_sequence == NEXTING_DEVICE_AUDIO_MAX_FRAMES &&
      audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING)
    return stop_stream(audio, NEXTING_DEVICE_AUDIO_END_MAX_DURATION);
  return true;
}

void nexting_ahakey_audio_on_hardware_error(nexting_ahakey_audio_t *audio) {
  cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_HARDWARE, true);
  render(audio, NEXTING_DEVICE_AUDIO_STATE_ERROR);
}

void nexting_ahakey_audio_tick(nexting_ahakey_audio_t *audio) {
  if (audio == NULL)
    return;
  const uint64_t now_ms = audio_now_ms(audio);
  if (audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING &&
      now_ms - audio->capture_started_ms >= 120000U) {
    (void)stop_stream(audio, NEXTING_DEVICE_AUDIO_END_MAX_DURATION);
    return;
  }
  if ((audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING ||
      audio->phase == NEXTING_AHAKEY_AUDIO_DRAINING) &&
      audio->queue_count > 0U && audio->credits == 0U &&
      audio->stall_active &&
      now_ms - audio->stall_started_ms >= NEXTING_AHAKEY_AUDIO_STALL_MS)
    cancel_stream(audio, NEXTING_DEVICE_AUDIO_CANCEL_TRANSPORT, true);
}

void nexting_ahakey_audio_disconnect(nexting_ahakey_audio_t *audio) {
  if (audio == NULL)
    return;
  const bool was_active = audio->phase == NEXTING_AHAKEY_AUDIO_CAPTURING ||
                          audio->phase == NEXTING_AHAKEY_AUDIO_DRAINING;
  audio->phase = NEXTING_AHAKEY_AUDIO_DRAINING;
  if (was_active && audio->hardware_cancel != NULL)
    audio->hardware_cancel(audio->hardware_context);
  audio->epoch = 0U;
  reset_stream(audio, audio->write_packet == NULL
                          ? NEXTING_AHAKEY_AUDIO_UNBOUND
                          : NEXTING_AHAKEY_AUDIO_UNCONFIGURED);
  render(audio, NEXTING_DEVICE_AUDIO_STATE_NONE);
}
