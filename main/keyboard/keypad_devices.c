#include "keypad_devices.h"
#include "keypad_model.h"
#include <string.h>

void kp_devices_defaults(kp_devices_t *d) {
    memset(d, 0, sizeof(*d)); d->version = 1; d->target = KP_DEVICE_NONE;
    for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) d->slot[i].system = KP_SYSTEM_UNSET;
}
unsigned kp_devices_count(const kp_devices_t *d) {
    unsigned count = 0;
    for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) count += d->slot[i].occupied != 0;
    return count;
}
int kp_devices_find(const kp_devices_t *d, const uint8_t identity[7]) {
    for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i)
        if (d->slot[i].occupied && !memcmp(d->slot[i].identity, identity, 7)) return (int)i;
    return -1;
}
bool kp_devices_valid(const kp_devices_t *d) {
    if (!d || d->version != 1 || (d->target != KP_DEVICE_NONE &&
        (d->target >= KP_DEVICE_COUNT || !d->slot[d->target].occupied))) return false;
    for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) {
        const kp_device_t *s = &d->slot[i];
        if (s->occupied > 1 || (s->system != KP_SYSTEM_UNSET && s->system >= KP_SYSTEM_COUNT) ||
            !memchr(s->name, 0, sizeof(s->name)) ||
            (*s->name && !kp_text_valid(s->name, 12, false))) return false;
        if (s->occupied && s->identity[0] > 1) return false;
        if (!s->occupied && (s->system != KP_SYSTEM_UNSET || *s->name)) return false;
    }
    return !d->slot[0].occupied || !d->slot[1].occupied ||
           memcmp(d->slot[0].identity, d->slot[1].identity, 7) != 0;
}
int kp_devices_reconcile(kp_devices_t *d, const uint8_t peers[][7], unsigned count) {
    if (count > KP_DEVICE_COUNT) return -1;
    for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) {
        bool found = false;
        for (unsigned j = 0; j < count; ++j)
            if (!memcmp(d->slot[i].identity, peers[j], 7)) found = true;
        if (d->slot[i].occupied && !found) {
            memset(&d->slot[i], 0, sizeof(d->slot[i])); d->slot[i].system = KP_SYSTEM_UNSET;
            if (d->target == i) d->target = KP_DEVICE_NONE;
        }
    }
    int added = -1;
    for (unsigned j = 0; j < count; ++j) {
        if (kp_devices_find(d, peers[j]) >= 0) continue;
        for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) if (!d->slot[i].occupied) {
            d->slot[i].occupied = 1; memcpy(d->slot[i].identity, peers[j], 7);
            d->slot[i].system = KP_SYSTEM_UNSET; added = (int)i; break;
        }
    }
    if (d->target == KP_DEVICE_NONE)
        for (unsigned i = 0; i < KP_DEVICE_COUNT; ++i) if (d->slot[i].occupied) { d->target = i; break; }
    return added;
}
const char *kp_device_name(const kp_devices_t *d, unsigned slot) {
    if (slot >= KP_DEVICE_COUNT) return "设备";
    return *d->slot[slot].name ? d->slot[slot].name : slot ? "设备 B" : "设备 A";
}
const char *kp_device_system(uint8_t system) {
    return system == KP_SYSTEM_MACOS ? "macOS" : system == KP_SYSTEM_WINDOWS ? "Windows" : "未选系统";
}
bool kp_device_can_send(const kp_devices_t *d, int actual, bool ready, bool editing, bool committed) {
    return committed && !editing && ready && actual >= 0 && actual < KP_DEVICE_COUNT &&
           d->target == (unsigned)actual && d->slot[actual].occupied && d->slot[actual].system < KP_SYSTEM_COUNT;
}
bool kp_device_name_set(kp_device_t *d, const char *name) {
    if (!d || !d->occupied || !name || !*name || strlen(name) >= sizeof(d->name) ||
        !kp_text_valid(name, 12, false)) return false;
    memset(d->name, 0, sizeof(d->name)); strcpy(d->name, name); return true;
}
