#include "keypad_web_presence.h"
#include <assert.h>
#include <stdio.h>
static void boundaries(uint64_t start) {
    kp_web_presence_t s;kp_web_presence_init(&s,start);
    assert(kp_web_presence_expired(&s,start));
    uint32_t a=kp_web_presence_join(&s,start),b=kp_web_presence_join(&s,start+1);
    assert(a&&b&&!kp_web_presence_expired(&s,start+1));
    kp_web_presence_leave(&s,a,start+2);assert(s.online==1);
    kp_web_presence_leave(&s,b,start+3);assert(kp_web_presence_expired(&s,start+3));
    kp_web_presence_leave(&s,b,start+4);assert(!s.online);
    a=kp_web_presence_join(&s,start+10);assert(!kp_web_presence_expired(&s,start+120009));
    assert(kp_web_presence_expired(&s,start+120010));assert(!kp_web_presence_ping(&s,a,start+120010));
    a=kp_web_presence_join(&s,start+200000);kp_web_presence_pause(&s,true,start+200001);
    assert(!kp_web_presence_expired(&s,start+1000000));
    kp_web_presence_pause(&s,false,start+1000001);assert(kp_web_presence_ping(&s,a,start+1000002));
    assert(!kp_web_presence_expired(&s,start+1120001));assert(kp_web_presence_expired(&s,start+1120002));
}
int main(void){boundaries(0);boundaries(UINT32_MAX-100);boundaries(UINT64_MAX-100);puts("Web presence: immediate last leave, multiple pages, exact heartbeat expiry, paused operations and wrap: PASS");}
