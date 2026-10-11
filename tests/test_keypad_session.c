#include "keypad_session.h"
#include <assert.h>
#include <stdio.h>
static void boundaries(uint64_t start) {
    kp_session_state_t s; kp_session_state_init(&s);
    uint32_t a=kp_session_state_open(&s,KP_SESSION_TRANSFER,start),g=s.generation;
    assert(a); kp_session_state_poll(&s,start+29999);assert(s.mode==KP_SESSION_TRANSFER);
    kp_session_state_poll(&s,start+30000);assert(s.mode==KP_SESSION_LISTEN);
    assert(!kp_session_state_join(&s,g,a,start+30000));
    a=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+40000);g=s.generation;
    assert(kp_session_state_join(&s,g,a,start+40001));
    kp_session_state_poll(&s,start+40000);assert(s.mode==KP_SESSION_TRANSFER); // Worker sample predates HTTP join.
    uint32_t b=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+40002);
    kp_session_state_leave(&s,g,a,start+40003);assert(s.mode==KP_SESSION_TRANSFER); // A second page is loading.
    assert(kp_session_state_join(&s,g,b,start+40004));
    a=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+40005);assert(kp_session_state_join(&s,g,a,start+40006));
    kp_session_state_leave(&s,g,b,start+40007);assert(s.mode==KP_SESSION_TRANSFER);
    kp_session_state_leave(&s,g,a,start+40008);assert(s.mode==KP_SESSION_LISTEN); // No grace.
    a=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+50000);g=s.generation;
    assert(kp_session_state_join(&s,g,a,start+50001));
    kp_session_state_poll(&s,start+170000);assert(s.mode==KP_SESSION_TRANSFER);
    kp_session_state_poll(&s,start+170001);assert(s.mode==KP_SESSION_LISTEN); // Heartbeat only, no extra minute.
    a=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+200000);g=s.generation;
    assert(kp_session_state_join(&s,g,a,start+200001));
    b=kp_session_state_open(&s,KP_SESSION_CONFIG,start+200002);uint32_t next=s.generation;
    assert(next!=g&&!kp_session_state_ping(&s,g,a,start+200003));
    kp_session_state_leave(&s,g,a,start+200003);assert(s.mode==KP_SESSION_CONFIG);
    assert(kp_session_state_join(&s,next,b,start+200004));
    kp_session_state_poll(&s,start+200001);assert(s.mode==KP_SESSION_CONFIG);
    uint32_t c=kp_session_state_open(&s,KP_SESSION_TRANSFER,start+200005);
    assert(!kp_session_state_join(&s,next,b,start+200006));
    assert(kp_session_state_join(&s,s.generation,c,start+200006));
}
int main(void) {
    boundaries(0);boundaries(UINT32_MAX-100);boundaries(UINT64_MAX-100);
    kp_session_reset();uint32_t g,ids[KP_SESSION_PAGES];
    for(unsigned i=0;i<KP_SESSION_PAGES;++i){ids[i]=kp_session_open(KP_SESSION_CONFIG,i,&g);assert(ids[i]);}
    uint32_t full_generation;assert(!kp_session_open(KP_SESSION_CONFIG,5,&full_generation)&&full_generation==g);
    assert(kp_session_join(KP_SESSION_CONFIG,g,ids[0],6));
    kp_session_pause(g,true,7);kp_session_poll(100000);assert(kp_session_current(KP_SESSION_CONFIG,g));
    kp_session_pause(g,false,100001);assert(kp_session_ping(KP_SESSION_CONFIG,g,ids[0],100002));
    kp_session_poll(100001);assert(kp_session_current(KP_SESSION_CONFIG,g));
    kp_session_end(KP_SESSION_CONFIG);assert(!kp_session_reserve(KP_SESSION_CONFIG,g,100003));
    uint32_t next,lease=kp_session_open(KP_SESSION_TRANSFER,100004,&next);assert(lease&&next!=g);
    assert(!kp_session_reserve(KP_SESSION_CONFIG,g,100005));
    kp_session_ready(KP_SESSION_CONFIG,g,true);kp_session_view_t v;kp_session_get(&v);assert(!v.ready);
    kp_session_ready(KP_SESSION_TRANSFER,next,true);kp_session_get(&v);assert(v.ready&&v.joining==1&&!v.online);
    // Opening from the device menu must not leave a phantom startup page.
    for(unsigned mode=KP_SESSION_TRANSFER;mode<=KP_SESSION_CONFIG;++mode) {
        kp_session_reset();kp_session_prepare(mode,0);kp_session_get(&v);assert(v.joining==1);
        lease=kp_session_open(mode,1,&g);assert(lease);kp_session_get(&v);assert(v.joining==1);
        assert(kp_session_join(mode,g,lease,2));kp_session_prepare(mode,3);
        kp_session_get(&v);assert(v.online==1&&!v.joining);
        kp_session_leave(mode,g,lease,4);kp_session_get(&v);assert(v.mode==KP_SESSION_LISTEN);
    }
    puts("LAN sessions: immediate last leave, 30s admission, 120s heartbeat, multiple pages, capacity, bidirectional generations, guarded retries, pause and unsigned wrap: PASS");
}
