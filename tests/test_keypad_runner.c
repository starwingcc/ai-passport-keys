#include "keypad_runner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool request(kp_runner_t *r,uint32_t now,unsigned modifiers,unsigned key) {
    kp_action_t a;if(!kp_runner_request(r,now,&a))return false;
    assert(a.modifiers==modifiers&&a.keys[0]==key);return true;
}
int main(void) {
    kp_config_t c;kp_defaults(&c);kp_runner_t r={0};
    kp_step_t steps[2]={{{.type=KP_KEYBOARD,.modifiers=8,.keys={4}},100},{{.type=KP_KEYBOARD,.keys={0x4c}},0}};
    assert(kp_binding_set(&c,0,3,"全选后删除","删除当前内容",steps,2));
    const kp_binding_t *b=kp_binding(&c,3);
    assert(kp_runner_start(&r,&c,b,false));assert(!kp_runner_start(&r,&c,b,false));
    assert(request(&r,0,8,4));assert(!request(&r,10,0,0)); // one in-flight report
    kp_runner_result(&r,true,25); // deadlines use acknowledgement time, not scheduling time
    assert(!request(&r,64,0,0));assert(request(&r,65,0,0));kp_runner_result(&r,true,70);
    assert(r.completed==1&&r.running);assert(!request(&r,169,0,0));
    assert(request(&r,170,0,0x4c));kp_runner_result(&r,true,175);
    assert(request(&r,215,0,0));kp_runner_result(&r,true,216);assert(!r.running&&!r.failed&&r.completed==2);
    char label[100];kp_binding_label(&c,b,label,sizeof(label));assert(!strcmp(label,"Command+A → Delete"));
    kp_binding_title(&c,b,label,sizeof(label));assert(!strcmp(label,"全选后删除"));
    // Multi-step holds run once and physical release cannot truncate the macro.
    assert(kp_runner_start(&r,&c,b,true)&&!r.hold);assert(request(&r,0,8,4));kp_runner_result(&r,true,0);
    kp_runner_release(&r,10);assert(!request(&r,39,0,0));assert(request(&r,40,0,0));kp_runner_result(&r,true,40);
    assert(request(&r,140,0,0x4c));kp_runner_result(&r,true,140);assert(request(&r,180,0,0));kp_runner_result(&r,true,180);assert(!r.running);
    // Single-step hold releases only on the physical edge.
    b=kp_binding(&c,2);assert(kp_runner_start(&r,&c,b,true));assert(request(&r,0,4,0));kp_runner_result(&r,true,0);
    assert(!request(&r,10000,0,0));kp_runner_release(&r,10001);assert(request(&r,10001,0,0));kp_runner_result(&r,true,10001);assert(!r.running);
    // Release arriving in the same input batch as the hold becomes a bounded tap.
    assert(kp_runner_start(&r,&c,b,true));kp_runner_release(&r,0);assert(!r.hold);
    assert(request(&r,0,4,0));kp_runner_result(&r,true,0);assert(request(&r,40,0,0));kp_runner_result(&r,true,40);assert(!r.running);
    // Failure and cancellation never replay completed steps.
    b=kp_binding(&c,3);assert(kp_runner_start(&r,&c,b,false));assert(request(&r,0,8,4));kp_runner_result(&r,false,1);
    assert(r.failed&&!r.running&&!request(&r,100,0,0));
    assert(kp_runner_start(&r,&c,b,false));assert(request(&r,0,8,4));kp_runner_result(&r,true,0);kp_runner_cancel(&r);
    assert(!request(&r,1000,0,0));
    // Wrap around and zero delay.
    steps[0].delay_ms=0;assert(kp_binding_set(&c,0,3,"","",steps,2));b=kp_binding(&c,3);
    assert(kp_runner_start(&r,&c,b,false));assert(request(&r,UINT32_MAX-20,8,4));kp_runner_result(&r,true,UINT32_MAX-20);
    assert(!request(&r,18,0,0));assert(request(&r,19,0,0));kp_runner_result(&r,true,19);
    assert(request(&r,19,0,0x4c));kp_runner_result(&r,true,19);assert(request(&r,59,0,0));kp_runner_result(&r,true,59);
    assert(!r.running&&r.completed==2);
    puts("Keypad macro timing, acknowledgement, hold, cancellation and failure: PASS");
}
