#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Transport mechanism only: each caller supplies its own stall/total policy.
// Owned by the HTTPD task; neither this state nor the transport allocates payloads.
typedef struct {
    uint32_t started_ms, progressed_ms, stall_ms, total_ms;
    size_t sent;
    unsigned waits;
    int error, blocked_error;
} kp_socket_io_t;
void kp_socket_io_begin(kp_socket_io_t *io, uint32_t stall_ms, uint32_t total_ms);
// HTTPD send override contract: return the entire length or a negative error.
int kp_socket_io_send(kp_socket_io_t *io, int fd, const char *data, size_t size);
// Release unsent/unacknowledged data on failed sessions before shutdown detaches
// the lwIP PCB. HTTPD still owns the descriptor and performs the final close.
bool kp_socket_io_abort(int fd);
