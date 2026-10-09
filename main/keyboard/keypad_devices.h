#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KP_DEVICE_COUNT 2
#define KP_DEVICE_NONE 255
#define KP_SYSTEM_UNSET 255
#define KP_DEVICE_NAME_BYTES 49
// Identity address type followed by six bytes. Never store the transient RPA.
typedef struct {
    uint8_t occupied, identity[7], system;
    char name[KP_DEVICE_NAME_BYTES];
} kp_device_t;
typedef struct {
    uint8_t version, target;
    kp_device_t slot[KP_DEVICE_COUNT];
} kp_devices_t;
void kp_devices_defaults(kp_devices_t *devices);
bool kp_devices_valid(const kp_devices_t *devices);
unsigned kp_devices_count(const kp_devices_t *devices);
int kp_devices_find(const kp_devices_t *devices, const uint8_t identity[7]);
// Reconcile against the authoritative bond store, retaining each identity's
// slot, name and explicit system. Unknown identities start UNSET, never evict.
int kp_devices_reconcile(kp_devices_t *devices, const uint8_t peers[][7], unsigned count);
const char *kp_device_name(const kp_devices_t *devices, unsigned slot);
const char *kp_device_system(uint8_t system);
bool kp_device_can_send(const kp_devices_t *devices, int actual, bool hid_ready,
                        bool editing, bool committed);
// Reject unsupported UTF-8, controls and overlong names instead of drawing boxes.
bool kp_device_name_set(kp_device_t *device, const char *name);
