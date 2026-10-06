#include "keypad_transfer_io.h"
#include "esp_timer.h"
#include <errno.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/socket.h>

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void kp_transfer_io_begin(kp_transfer_io_t *io) {
    kp_transfer_io_begin_timed(io, KP_TRANSFER_SEND_MS, KP_TRANSFER_SEND_MS);
}

void kp_transfer_io_begin_timed(kp_transfer_io_t *io, uint32_t stall_ms, uint32_t total_ms) {
    uint32_t now = now_ms();
    *io = (kp_transfer_io_t){.started_ms = now, .progressed_ms = now,
                            .stall_ms = stall_ms, .total_ms = total_ms};
}

static uint32_t remaining_ms(const kp_transfer_io_t *io) {
    uint32_t now = now_ms(), total = now - io->started_ms, stalled = now - io->progressed_ms;
    if (total >= io->total_ms || stalled >= io->stall_ms) return 0;
    uint32_t remaining = io->total_ms - total;
    return remaining < io->stall_ms - stalled ? remaining : io->stall_ms - stalled;
}

int kp_transfer_io_send(kp_transfer_io_t *io, int fd, const char *data, size_t size) {
    if (io->error) return -1;
    if (fd < 0 || fd >= FD_SETSIZE || size > INT_MAX) {
        io->error = EINVAL;
        return -1;
    }
    size_t offset = 0;
    while (offset < size) {
        if (!remaining_ms(io)) { io->error = ETIMEDOUT; return -1; }
        // Limit each copy into lwIP to one configured MSS. A short write is
        // progress, not a complete WebSocket frame (IDF 5.5.3 assumes the latter).
        size_t count = size - offset;
        if (count > 1440) count = 1440;
        int written = send(fd, data + offset, count, MSG_DONTWAIT);
        if (written > 0) {
            offset += (size_t)written; io->sent += (size_t)written;
            io->progressed_ms = now_ms();
            continue;
        }
        if (written == 0) { io->error = ECONNRESET; return -1; }
        int error = errno;
        if (error == EINTR) continue;
        if (error != EAGAIN && error != EWOULDBLOCK && error != ENOMEM && error != ENOBUFS) {
            io->error = error;
            return -1;
        }
        io->blocked_error = error;
        ++io->waits;
        // lwIP may still report writable after an allocation failure. Bound
        // retries even in that case, rather than spinning on select readiness.
        uint32_t remaining = remaining_ms(io);
        if (!remaining) { io->error = ETIMEDOUT; return -1; }
        // select's writable low-water mark can exceed the space needed for a
        // short send (lwIP with a two-MSS buffer requires it to be empty).
        // Readiness is only a hint: periodically retry send, and let actual
        // progress and the transaction deadline decide whether it timed out.
        if (remaining > 100) remaining = 100;
        struct timeval wait = {.tv_usec = remaining * 1000};
        fd_set writable;
        FD_ZERO(&writable); FD_SET(fd, &writable);
        int ready = select(fd + 1, NULL, &writable, NULL, &wait);
        if (ready < 0 && errno != EINTR) { io->error = errno; return -1; }
        // Readiness is advisory. On a repeated allocation failure, returning
        // control briefly also lets tcpip/Wi-Fi reclaim their packet buffers.
        if (ready > 0) {
            struct timeval pause = {.tv_usec = 1000};
            select(0, NULL, NULL, NULL, &pause);
        }
    }
    return (int)size;
}

bool kp_transfer_io_abort(int fd) {
    const struct linger discard = {.l_onoff = 1, .l_linger = 0};
    bool configured = setsockopt(fd, SOL_SOCKET, SO_LINGER, &discard, sizeof(discard)) == 0;
    shutdown(fd, SHUT_RDWR);
    return configured;
}
