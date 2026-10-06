#include "keypad_transfer_io.h"
#include "esp_timer.h"
#include <errno.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/socket.h>

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void kp_transfer_io_begin(kp_transfer_io_t *io) {
    *io = (kp_transfer_io_t){.started_ms = now_ms()};
}

int kp_transfer_io_send(kp_transfer_io_t *io, int fd, const char *data, size_t size) {
    if (io->error) return -1;
    if (fd < 0 || fd >= FD_SETSIZE || size > INT_MAX) {
        io->error = EINVAL;
        return -1;
    }
    size_t offset = 0;
    while (offset < size) {
        uint32_t elapsed = now_ms() - io->started_ms;
        if (elapsed >= KP_TRANSFER_SEND_MS) { io->error = ETIMEDOUT; return -1; }
        // Limit each copy into lwIP to one configured MSS. A short write is
        // progress, not a complete WebSocket frame (IDF 5.5.3 assumes the latter).
        size_t count = size - offset;
        if (count > 1440) count = 1440;
        int written = send(fd, data + offset, count, MSG_DONTWAIT);
        if (written > 0) { offset += (size_t)written; io->sent += (size_t)written; continue; }
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
        elapsed = now_ms() - io->started_ms;
        if (elapsed >= KP_TRANSFER_SEND_MS) { io->error = ETIMEDOUT; return -1; }
        uint32_t remaining = KP_TRANSFER_SEND_MS - elapsed;
        struct timeval wait = {.tv_sec = remaining / 1000, .tv_usec = (remaining % 1000) * 1000};
        fd_set writable;
        FD_ZERO(&writable); FD_SET(fd, &writable);
        int ready = select(fd + 1, NULL, &writable, NULL, &wait);
        if (ready < 0 && errno != EINTR) { io->error = errno; return -1; }
        if (!ready) { io->error = ETIMEDOUT; return -1; }
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
