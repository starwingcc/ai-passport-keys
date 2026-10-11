#include "keypad_transfer_lifecycle.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    kp_transfer_lifecycle_t s={0};kp_transfer_lifecycle_enable(&s,0);
    assert(!kp_transfer_lifecycle_expired(&s,0));
    kp_transfer_presence_t p={.allocated=true,.joining=1};kp_transfer_lifecycle_observe(&s,&p);
    assert(!kp_transfer_lifecycle_expired(&s,300));
    p.joining=0;p.online=1;kp_transfer_lifecycle_observe(&s,&p);assert(s.phase==KP_TRANSFER_RUN);
    p.online=0;kp_transfer_lifecycle_observe(&s,&p);assert(kp_transfer_lifecycle_expired(&s,301));
    p.joining=1;kp_transfer_lifecycle_observe(&s,&p);assert(!kp_transfer_lifecycle_expired(&s,301));
    kp_transfer_lifecycle_reset(&s);assert(s.phase==KP_TRANSFER_LISTEN);
    puts("Transfer lifecycle: pending admission, online lifetime and immediate offline cleanup: PASS");
}
