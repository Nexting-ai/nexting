#include "nexting_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  size_t count;
  nexting_device_message_t last;
} capture_t;

static void capture_message(const nexting_device_message_t *message,
                            void *context) {
  capture_t *capture = context;
  capture->count += 1;
  capture->last = *message;
}

static void accepts_fragments_and_multiple_frames(void) {
  uint8_t storage[512];
  nexting_device_stream_t stream;
  capture_t capture = {0};
  const char *first = "{\"v\":1,\"t\":\"answer\",\"id\":\"r1\",\"ch\":\"al";
  const char *second =
      "low\"}\n{\"v\":1,\"t\":\"answer\",\"id\":\"r2\",\"ch\":\"deny\"}\n";
  nexting_device_stream_init(&stream, storage, sizeof storage);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)first,
                                    strlen(first), capture_message,
                                    &capture) == NEXTING_DEVICE_OK);
  assert(capture.count == 0);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)second,
                                    strlen(second), capture_message,
                                    &capture) == NEXTING_DEVICE_OK);
  assert(capture.count == 2);
  assert(strcmp(capture.last.request_id, "r2") == 0);
}

static void rejects_oversize_stream_and_recovers(void) {
  uint8_t storage[64];
  uint8_t oversized[80];
  nexting_device_stream_t stream;
  capture_t capture = {0};
  const char *valid = "{\"v\":1,\"t\":\"error\",\"code\":\"busy\"}\n";
  memset(oversized, 'x', sizeof oversized);
  nexting_device_stream_init(&stream, storage, sizeof storage);
  assert(nexting_device_stream_push(&stream, oversized, sizeof oversized,
                                    capture_message, &capture) ==
         NEXTING_DEVICE_MESSAGE_TOO_LARGE);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)"\n", 1,
                                    capture_message, &capture) ==
         NEXTING_DEVICE_MESSAGE_TOO_LARGE);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)valid,
                                    strlen(valid), capture_message,
                                    &capture) == NEXTING_DEVICE_OK);
  assert(capture.count == 1);
}

static void reset_discards_partial_frame(void) {
  uint8_t storage[128];
  nexting_device_stream_t stream;
  capture_t capture = {0};
  const char *partial = "{\"v\":1,\"t\":\"answer\"";
  const char *valid = "{\"v\":1,\"t\":\"error\",\"code\":\"busy\"}\n";
  nexting_device_stream_init(&stream, storage, sizeof storage);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)partial,
                                    strlen(partial), capture_message,
                                    &capture) == NEXTING_DEVICE_OK);
  nexting_device_stream_reset(&stream);
  assert(nexting_device_stream_push(&stream, (const uint8_t *)valid,
                                    strlen(valid), capture_message,
                                    &capture) == NEXTING_DEVICE_OK);
  assert(capture.count == 1);
}

static void frame_limit_counts_terminating_newline(void) {
  uint8_t exact_storage[64];
  uint8_t small_storage[64];
  nexting_device_stream_t exact;
  nexting_device_stream_t too_small;
  capture_t exact_capture = {0};
  capture_t small_capture = {0};
  const char *wire = "{\"v\":1,\"t\":\"error\",\"code\":\"busy\"}\n";
  const size_t frame_length = strlen(wire);

  nexting_device_stream_init(&exact, exact_storage, frame_length);
  assert(nexting_device_stream_push(&exact, (const uint8_t *)wire,
                                    frame_length, capture_message,
                                    &exact_capture) == NEXTING_DEVICE_OK);
  assert(exact_capture.count == 1);

  nexting_device_stream_init(&too_small, small_storage, frame_length - 1);
  assert(nexting_device_stream_push(&too_small, (const uint8_t *)wire,
                                    frame_length, capture_message,
                                    &small_capture) ==
         NEXTING_DEVICE_MESSAGE_TOO_LARGE);
  assert(small_capture.count == 0);
}

int main(void) {
  accepts_fragments_and_multiple_frames();
  rejects_oversize_stream_and_recovers();
  reset_discards_partial_frame();
  frame_limit_counts_terminating_newline();
  puts("stream tests passed");
  return 0;
}
