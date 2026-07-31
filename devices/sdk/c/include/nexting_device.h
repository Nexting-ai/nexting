#ifndef NEXTING_DEVICE_H
#define NEXTING_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXTING_DEVICE_REQUEST_ID_CAPACITY 65
#define NEXTING_DEVICE_SUMMARY_CAPACITY 241
#define NEXTING_DEVICE_DEFAULT_MAX_MESSAGE_BYTES 4096
#define NEXTING_DEVICE_MAX_TTL_MS 300000U
#define NEXTING_DEVICE_ANSWER_RETRY_MS 1000U
#define NEXTING_DEVICE_STATUS_MAX_AGENTS 8U
#define NEXTING_DEVICE_STATUS_LABEL_CAPACITY 65
#define NEXTING_DEVICE_INFO_STRING_CAPACITY 65
#define NEXTING_DEVICE_INFO_UUID_CAPACITY 37
#define NEXTING_DEVICE_INFO_MAX_WIRE_VERSIONS 4
#define NEXTING_DEVICE_INFO_MAX_PROFILES 16
#define NEXTING_DEVICE_INFO_PROFILE_CAPACITY 33
#define NEXTING_DEVICE_INFO_MAX_HAPTICS 8
#define NEXTING_DEVICE_INFO_HAPTIC_CAPACITY 33
#define NEXTING_DEVICE_INFO_MAX_VENDOR_FACTS 16
#define NEXTING_DEVICE_INFO_VENDOR_NAMESPACE_CAPACITY 129
#define NEXTING_DEVICE_INFO_VENDOR_KEY_CAPACITY 33
#define NEXTING_DEVICE_INFO_VENDOR_LABEL_CAPACITY 65
#define NEXTING_DEVICE_INFO_VENDOR_VALUE_CAPACITY 129

typedef enum {
  NEXTING_DEVICE_OK = 0,
  NEXTING_DEVICE_BAD_MESSAGE,
  NEXTING_DEVICE_MESSAGE_TOO_LARGE,
  NEXTING_DEVICE_BUFFER_TOO_SMALL
} nexting_device_result_t;

typedef enum {
  NEXTING_DEVICE_MESSAGE_NONE = 0,
  NEXTING_DEVICE_MESSAGE_PRESENT,
  NEXTING_DEVICE_MESSAGE_ANSWER,
  NEXTING_DEVICE_MESSAGE_RESOLVED,
  NEXTING_DEVICE_MESSAGE_ERROR,
  NEXTING_DEVICE_MESSAGE_STATUS
} nexting_device_message_type_t;

typedef enum {
  NEXTING_DEVICE_CHOICE_NONE = 0,
  NEXTING_DEVICE_CHOICE_ALLOW,
  NEXTING_DEVICE_CHOICE_DENY
} nexting_device_choice_t;

typedef enum {
  NEXTING_DEVICE_RESOLUTION_NONE = 0,
  NEXTING_DEVICE_RESOLUTION_ANSWERED,
  NEXTING_DEVICE_RESOLUTION_EXPIRED,
  NEXTING_DEVICE_RESOLUTION_CANCELLED,
  NEXTING_DEVICE_RESOLUTION_REPLACED
} nexting_device_resolution_t;

typedef enum {
  NEXTING_DEVICE_ERROR_NONE = 0,
  NEXTING_DEVICE_ERROR_BAD_MESSAGE,
  NEXTING_DEVICE_ERROR_MESSAGE_TOO_LARGE,
  NEXTING_DEVICE_ERROR_UNSUPPORTED_VERSION,
  NEXTING_DEVICE_ERROR_UNSUPPORTED_PROFILE,
  NEXTING_DEVICE_ERROR_UNKNOWN_REQUEST,
  NEXTING_DEVICE_ERROR_NOT_AUTHORIZED,
  NEXTING_DEVICE_ERROR_BUSY
} nexting_device_error_code_t;

typedef enum {
  NEXTING_DEVICE_AGENT_STATE_NONE = 0,
  NEXTING_DEVICE_AGENT_STATE_IDLE,
  NEXTING_DEVICE_AGENT_STATE_THINKING,
  NEXTING_DEVICE_AGENT_STATE_WORKING,
  NEXTING_DEVICE_AGENT_STATE_COMPLETE,
  NEXTING_DEVICE_AGENT_STATE_NEEDS_INPUT,
  NEXTING_DEVICE_AGENT_STATE_ERROR
} nexting_device_agent_state_t;

typedef struct {
  uint8_t slot;
  nexting_device_agent_state_t state;
  bool has_label;
  char label[NEXTING_DEVICE_STATUS_LABEL_CAPACITY];
} nexting_device_agent_status_t;

typedef struct {
  nexting_device_message_type_t type;
  bool has_request_id;
  char request_id[NEXTING_DEVICE_REQUEST_ID_CAPACITY];
  char summary[NEXTING_DEVICE_SUMMARY_CAPACITY];
  uint32_t ttl_ms;
  nexting_device_choice_t choice;
  nexting_device_resolution_t resolution;
  nexting_device_error_code_t error_code;
  size_t agent_count;
  nexting_device_agent_status_t agents[NEXTING_DEVICE_STATUS_MAX_AGENTS];
} nexting_device_message_t;

nexting_device_result_t nexting_device_decode(const char *wire,
                                              size_t wire_length,
                                              nexting_device_message_t *output);
nexting_device_result_t
nexting_device_encode(const nexting_device_message_t *message, char *output,
                      size_t output_capacity, size_t *output_length);

typedef struct {
  char key[NEXTING_DEVICE_INFO_VENDOR_KEY_CAPACITY];
  char label[NEXTING_DEVICE_INFO_VENDOR_LABEL_CAPACITY];
  char value[NEXTING_DEVICE_INFO_VENDOR_VALUE_CAPACITY];
} nexting_device_vendor_fact_t;

typedef struct {
  char protocol_name[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  char spec[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  size_t wire_version_count;
  uint16_t wire_versions[NEXTING_DEVICE_INFO_MAX_WIRE_VERSIONS];
  size_t profile_count;
  char profiles[NEXTING_DEVICE_INFO_MAX_PROFILES]
               [NEXTING_DEVICE_INFO_PROFILE_CAPACITY];
  char model[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  char firmware_version[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  uint32_t max_message_bytes;
  uint16_t max_summary_bytes;
  char device_id[NEXTING_DEVICE_INFO_UUID_CAPACITY];
  char manufacturer[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  char display_name[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  char serial_number[NEXTING_DEVICE_INFO_STRING_CAPACITY];
  bool has_button_count;
  uint16_t button_count;
  bool has_approval_button_count;
  uint16_t approval_button_count;
  bool has_custom_button_count;
  uint16_t custom_button_count;
  bool has_rotary_count;
  uint8_t rotary_count;
  bool has_rotary_press_count;
  uint8_t rotary_press_count;
  uint8_t status_slots;
  bool has_battery_service;
  bool has_display;
  char display_type[NEXTING_DEVICE_INFO_PROFILE_CAPACITY];
  uint16_t display_width;
  uint16_t display_height;
  size_t haptic_count;
  char haptics[NEXTING_DEVICE_INFO_MAX_HAPTICS]
               [NEXTING_DEVICE_INFO_HAPTIC_CAPACITY];
  bool has_vendor;
  char vendor_namespace[NEXTING_DEVICE_INFO_VENDOR_NAMESPACE_CAPACITY];
  size_t vendor_fact_count;
  nexting_device_vendor_fact_t
      vendor_facts[NEXTING_DEVICE_INFO_MAX_VENDOR_FACTS];
  bool supports_approval_v1;
  bool supports_status_v1;
} nexting_device_info_t;

nexting_device_result_t
nexting_device_info_decode(const char *wire, size_t wire_length,
                           nexting_device_info_t *output);

typedef void (*nexting_device_message_handler_t)(
    const nexting_device_message_t *message, void *context);

typedef struct {
  uint8_t *storage;
  size_t capacity;
  size_t length;
  bool discarding_oversize;
} nexting_device_stream_t;

void nexting_device_stream_init(nexting_device_stream_t *stream,
                                uint8_t *storage, size_t capacity);
void nexting_device_stream_reset(nexting_device_stream_t *stream);
nexting_device_result_t nexting_device_stream_push(
    nexting_device_stream_t *stream, const uint8_t *bytes, size_t length,
    nexting_device_message_handler_t handler, void *context);

typedef enum {
  NEXTING_DEVICE_PHASE_IDLE = 0,
  NEXTING_DEVICE_PHASE_PENDING,
  NEXTING_DEVICE_PHASE_WAITING_RESOLUTION
} nexting_device_phase_t;

typedef struct {
  nexting_device_phase_t phase;
  nexting_device_message_t request;
  uint64_t deadline_ms;
  uint64_t next_retry_ms;
  nexting_device_choice_t chosen;
} nexting_device_state_t;

void nexting_device_state_init(nexting_device_state_t *state);
nexting_device_result_t
nexting_device_state_on_present(nexting_device_state_t *state,
                                const nexting_device_message_t *message,
                                uint64_t now_ms);
nexting_device_result_t
nexting_device_state_choose(nexting_device_state_t *state,
                            nexting_device_choice_t choice, uint64_t now_ms,
                            nexting_device_message_t *answer);
bool nexting_device_state_retry_answer(nexting_device_state_t *state,
                                       uint64_t now_ms,
                                       nexting_device_message_t *answer);
nexting_device_result_t
nexting_device_state_on_resolved(nexting_device_state_t *state,
                                 const nexting_device_message_t *message);
bool nexting_device_state_tick(nexting_device_state_t *state, uint64_t now_ms);
void nexting_device_state_disconnect(nexting_device_state_t *state);

typedef struct {
  bool occupied[NEXTING_DEVICE_STATUS_MAX_AGENTS];
  nexting_device_agent_status_t slots[NEXTING_DEVICE_STATUS_MAX_AGENTS];
} nexting_device_status_state_t;

void nexting_device_status_init(nexting_device_status_state_t *state);
nexting_device_result_t
nexting_device_status_on_message(nexting_device_status_state_t *state,
                                 const nexting_device_message_t *message);
void nexting_device_status_disconnect(nexting_device_status_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
