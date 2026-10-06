#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// One HTTP response / WebSocket frame has one budget, including partial writes.
// Owned by the HTTPD task; neither this state nor the transport allocates payloads.
#define KP_TRANSFER_SEND_MS 2000U
typedef struct {
    uint32_t started_ms;
    size_t sent;
    unsigned waits;
    int error, blocked_error;
} kp_transfer_io_t;
void kp_transfer_io_begin(kp_transfer_io_t *io);
// HTTPD send override contract: return the entire length or a negative error.
int kp_transfer_io_send(kp_transfer_io_t *io, int fd, const char *data, size_t size);
// Release unsent/unacknowledged data on failed sessions before shutdown detaches
// the lwIP PCB. HTTPD still owns the descriptor and performs the final close.
bool kp_transfer_io_abort(int fd);
