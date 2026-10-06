#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// WebSocket frames retain their short total budget. HTTP responses can choose
// a longer total budget and a separate no-progress timeout for slow links.
// Owned by the HTTPD task; neither this state nor the transport allocates payloads.
#define KP_TRANSFER_SEND_MS 2000U
typedef struct {
    uint32_t started_ms, progressed_ms, stall_ms, total_ms;
    size_t sent;
    unsigned waits;
    int error, blocked_error;
} kp_transfer_io_t;
void kp_transfer_io_begin(kp_transfer_io_t *io);
void kp_transfer_io_begin_timed(kp_transfer_io_t *io, uint32_t stall_ms, uint32_t total_ms);
// HTTPD send override contract: return the entire length or a negative error.
int kp_transfer_io_send(kp_transfer_io_t *io, int fd, const char *data, size_t size);
// Release unsent/unacknowledged data on failed sessions before shutdown detaches
// the lwIP PCB. HTTPD still owns the descriptor and performs the final close.
bool kp_transfer_io_abort(int fd);
