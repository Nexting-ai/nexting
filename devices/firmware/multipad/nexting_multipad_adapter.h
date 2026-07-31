#ifndef NEXTING_MULTIPAD_ADAPTER_H
#define NEXTING_MULTIPAD_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nexting_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The ILX MultiPad application speaks USB CDC. This adapter deliberately
 * leaves USB, GPIO, displays, and clocks to the board port and only connects
 * them to the portable Nexting C99 state machine.
 */
#define NEXTING_MULTIPAD_STREAM_CAPACITY \
  (NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES + 1U)

typedef uint64_t (*nexting_multipad_now_ms_fn)(void *context);
typedef void (*nexting_multipad_write_frame_fn)(const uint8_t *bytes,
                                                size_t length, void *context);
typedef void (*nexting_multipad_render_approval_fn)(
    const nexting_device_state_t *state, void *context);
typedef void (*nexting_multipad_render_status_fn)(
    const nexting_device_status_state_t *state, void *context);

typedef struct {
  nexting_device_state_t approval;
  nexting_device_status_state_t status;
  nexting_device_stream_t stream;
  uint8_t stream_storage[NEXTING_MULTIPAD_STREAM_CAPACITY];
  bool frame_active;
  nexting_multipad_now_ms_fn now_ms;
  nexting_multipad_write_frame_fn write_frame;
  nexting_multipad_render_approval_fn render_approval;
  nexting_multipad_render_status_fn render_status;
  void *context;
} nexting_multipad_t;

void nexting_multipad_init(nexting_multipad_t *adapter,
                           nexting_multipad_now_ms_fn now_ms,
                           nexting_multipad_write_frame_fn write_frame,
                           nexting_multipad_render_approval_fn render_approval,
                           nexting_multipad_render_status_fn render_status,
                           void *context);

/*
 * Return true only for a JSON frame or a continuation of one. The upstream
 * MultiPad firmware retains its legacy AA BB xx commands; the USB callback
 * can use this gate before handing a buffer to nexting_multipad_receive().
 */
bool nexting_multipad_accepts(const nexting_multipad_t *adapter,
                              const uint8_t *bytes, size_t length);

nexting_device_result_t nexting_multipad_receive(nexting_multipad_t *adapter,
                                                 const uint8_t *bytes,
                                                 size_t length);

nexting_device_result_t nexting_multipad_choose(
    nexting_multipad_t *adapter, nexting_device_choice_t choice);

void nexting_multipad_tick(nexting_multipad_t *adapter);
void nexting_multipad_disconnect(nexting_multipad_t *adapter);

#ifdef __cplusplus
}
#endif

#endif
