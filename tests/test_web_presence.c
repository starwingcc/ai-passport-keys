#include "keypad_web_presence.h"
#include <assert.h>
#include <stdio.h>
static unsigned online(kp_web_presence_t *s,uint64_t now){kp_web_presence_poll(s,now);return s->online;}
static void boundaries(uint64_t start) {
    kp_web_presence_t s;kp_web_presence_init(&s);
    assert(!online(&s,start));
    uint32_t a=kp_web_presence_join_id(&s,1,start),b=kp_web_presence_join_id(&s,2,start+1);
    assert(a&&b&&online(&s,start+1)==2);
    assert(!kp_web_presence_join_id(&s,a,start+1));
    kp_web_presence_leave(&s,a,start+2);assert(s.online==1);
    kp_web_presence_leave(&s,b,start+3);assert(!online(&s,start+3));
    kp_web_presence_leave(&s,b,start+4);assert(!s.online);
    a=kp_web_presence_join_id(&s,3,start+10);assert(online(&s,start+120009)==1);
    assert(!online(&s,start+120010));assert(!kp_web_presence_ping(&s,a,start+120010));
    a=kp_web_presence_join_id(&s,4,start+200000);kp_web_presence_pause(&s,true,start+200001);
    assert(online(&s,start+1000000)==1);
    kp_web_presence_pause(&s,false,start+1000001);assert(kp_web_presence_ping(&s,a,start+1000002));
    assert(online(&s,start+1120001)==1);assert(!online(&s,start+1120002));
}
int main(void){boundaries(0);boundaries(UINT32_MAX-100);boundaries(UINT64_MAX-100);puts("Web presence: page leave, duplicates, exact heartbeat expiry, paused operations and wrap: PASS");}
