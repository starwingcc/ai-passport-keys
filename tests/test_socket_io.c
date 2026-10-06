#include <assert.h>
#include <stdio.h>
#include <string.h>
#define send test_send
#define select test_select
#define setsockopt test_setsockopt
#define shutdown test_shutdown
#include "../main/keyboard/keypad_socket_io.c"
#undef send
#undef select
#undef setsockopt
#undef shutdown

static uint32_t clock_ms;
static unsigned calls, mode, abort_stage;
static char received[8192];
static size_t received_size;
int64_t esp_timer_get_time(void) { return (int64_t)clock_ms*1000; }
ssize_t test_send(int fd,const void *data,size_t size,int flags) {
    assert(fd==41&&flags==MSG_DONTWAIT&&size<=1440);
    ++calls;
    if(mode==1) { errno=EAGAIN;return -1; }
    if(mode==2) { errno=ECONNRESET;return -1; }
    if(mode==3) { clock_ms+=600; }
    if(mode==4) return 0;
    if(mode==5) { errno=ENOMEM;return -1; }
    if(mode==6&&calls%2==1) { errno=EAGAIN;return -1; }
    if(!mode&&calls%3==0) { errno=EAGAIN;return -1; }
    if(size>113)size=113; // positive short writes, including midway through payload
    assert(received_size+size<=sizeof(received));
    memcpy(received+received_size,data,size);received_size+=size;
    return (ssize_t)size;
}
int test_select(int count,fd_set *r,fd_set *w,fd_set *x,struct timeval *timeout) {
    assert(!r&&!x&&timeout);
    if(!count) { assert(!w);clock_ms+=(uint32_t)(timeout->tv_usec/1000);return 0; }
    assert(count==42&&w&&FD_ISSET(41,w));
    if(mode==1||mode==6) { clock_ms+=(uint32_t)(timeout->tv_sec*1000+timeout->tv_usec/1000);return 0; }
    return 1; // includes spuriously writable while allocation fails
}
int test_setsockopt(int fd,int level,int option,const void *value,socklen_t size) {
    assert(fd==41&&level==SOL_SOCKET&&option==SO_LINGER&&size==sizeof(struct linger));
    const struct linger *linger=value;
    assert(linger->l_onoff==1&&linger->l_linger==0&&abort_stage==0);
    abort_stage=1;return 0;
}
int test_shutdown(int fd,int how) { assert(fd==41&&how==SHUT_RDWR&&abort_stage==1);abort_stage=2;return 0; }
static void reset(unsigned scenario) { mode=scenario;calls=0;received_size=0;clock_ms=0; }
int main(void) {
    char payload[4096];for(size_t i=0;i<sizeof(payload);++i)payload[i]=(char)i;
    kp_socket_io_t io;
    reset(0);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,"head",4)==4);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))==sizeof(payload));
    assert(received_size==4100&&!memcmp(received,"head",4)&&!memcmp(received+4,payload,sizeof(payload)));
    assert(io.sent==4100&&io.waits>0&&!io.error);
    reset(1);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))<0&&io.error==ETIMEDOUT&&clock_ms==2000);
    assert(!io.sent&&io.waits==20&&io.blocked_error==EAGAIN);
    reset(2);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,payload,1)<0&&io.error==ECONNRESET);
    unsigned before=calls;assert(kp_socket_io_send(&io,41,payload,1)<0&&calls==before);
    reset(3);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))<0&&io.error==ETIMEDOUT&&io.sent>0);
    assert(calls==4); // progress cannot renew a transaction's deadline
    // HTTP allows a slow response but still bounds a stalled receiver and the
    // total time spent on one client. Short budgets above exercise the WS policy.
    reset(3);kp_socket_io_begin(&io,5000,30000);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))==sizeof(payload));
    assert(clock_ms>2000&&io.sent==sizeof(payload)&&!memcmp(received,payload,sizeof(payload)));
    mode=1;before=clock_ms;
    assert(kp_socket_io_send(&io,41,payload,1)<0&&io.error==ETIMEDOUT&&clock_ms-before==5000);
    reset(3);kp_socket_io_begin(&io,5000,3000);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))<0&&io.error==ETIMEDOUT&&clock_ms==3000);
    reset(4);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,payload,1)<0&&io.error==ECONNRESET);
    reset(5);kp_socket_io_begin(&io,2000,2000);
    assert(kp_socket_io_send(&io,41,payload,1)<0&&io.error==ETIMEDOUT&&clock_ms==2000);
    assert(io.waits==2000&&io.blocked_error==ENOMEM); // no tight allocation spin
    // Space below select's low-water mark still permits positive short writes.
    reset(6);kp_socket_io_begin(&io,5000,30000);
    assert(kp_socket_io_send(&io,41,payload,sizeof(payload))==sizeof(payload));
    assert(clock_ms>2000&&io.waits>0&&!memcmp(received,payload,sizeof(payload)));
    reset(0);clock_ms=UINT32_MAX-5;kp_socket_io_begin(&io,2000,2000);clock_ms=10;
    assert(kp_socket_io_send(&io,41,payload,1)==1); // uptime wrap is not expiry
    assert(kp_socket_io_abort(41)&&abort_stage==2);
    puts("Socket IO: partial writes, backpressure, total deadline, EOF/reset, memory pressure, clock wrap and abort-before-shutdown: PASS");
}
