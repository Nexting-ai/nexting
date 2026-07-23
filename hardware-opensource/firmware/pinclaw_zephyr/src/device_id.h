#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <stdint.h>

/**
 * Initialize device ID from NVS.
 * Must be called before transport_start().
 */
int device_id_init(void);

/**
 * Get the BLE device name (e.g. "Pinclaw 010").
 * Returns pointer to static buffer, valid until next set call.
 */
const char *device_id_get_name(void);

/**
 * Get the name length.
 */
uint8_t device_id_get_name_len(void);

/**
 * Set device number (0-999) and persist to NVS.
 * Updates the BLE name immediately.
 * Returns 0 on success.
 */
int device_id_set(uint16_t number);

/**
 * Get the raw device number (0 if not set).
 */
uint16_t device_id_get_number(void);

/**
 * Get the 16-char hex hardware ID derived from FICR DEVICEID.
 * Example: "A1B2C3D4E5F60718"
 * Returns pointer to static buffer, always valid after device_id_init().
 */
const char *device_id_get_hardware_id(void);

#endif
