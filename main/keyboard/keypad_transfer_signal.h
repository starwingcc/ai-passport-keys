#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define KP_TRANSFER_PEERS 4
#define KP_TRANSFER_MESSAGE 4096
#define KP_TRANSFER_CATALOG 2304
#define KP_TRANSFER_IDLE_MS 120000U
// Pure bounded signaling state; file bytes and text content are never accepted.
typedef struct {
    int fd;
    uint32_t id;
    uint64_t last_ms;
    bool joined, rtc;
    char name[49];
    // NULL is an empty catalog. Allocate only actual serialized metadata;
    // reserving four maximum-size catalogs starves HTTP/Wi-Fi at page load.
    char *catalog;
} kp_transfer_peer_t;
typedef bool (*kp_transfer_send_t)(void *user, int fd, const char *data, size_t size);
typedef struct {
    kp_transfer_peer_t peers[KP_TRANSFER_PEERS];
    uint32_t next_id;
    kp_transfer_send_t send;
    void *user;
} kp_transfer_signal_t;
void kp_transfer_signal_init(kp_transfer_signal_t *s, kp_transfer_send_t send, void *user);
// Release all owned catalogs without sending messages. Call before reinit/free.
void kp_transfer_signal_clear(kp_transfer_signal_t *s);
// Retire a peer without publishing, for transport teardown after the end frame.
void kp_transfer_signal_discard(kp_transfer_signal_t *s, int fd);
bool kp_transfer_signal_open(kp_transfer_signal_t *s, int fd, uint64_t now);
void kp_transfer_signal_close(kp_transfer_signal_t *s, int fd);
bool kp_transfer_signal_message(kp_transfer_signal_t *s, int fd, const char *data, size_t size, uint64_t now);
unsigned kp_transfer_signal_count(const kp_transfer_signal_t *s);
unsigned kp_transfer_signal_online(const kp_transfer_signal_t *s, uint64_t now);
// Return one expired descriptor at a time; transport closes it, retaining ordering.
int kp_transfer_signal_expired(const kp_transfer_signal_t *s, uint64_t now);
void kp_transfer_signal_end(kp_transfer_signal_t *s);
