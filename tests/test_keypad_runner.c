#include "keypad_runner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool request(kp_runner_t *r,uint32_t now,unsigned modifiers,unsigned key) {
    kp_action_t a;if(!kp_runner_request(r,now,&a))return false;
    assert(a.modifiers==modifiers&&a.keys[0]==key);return true;
}
typedef struct { kp_model_t model; kp_runner_t runner; uint32_t now; unsigned starts, releases, cancels; } runtime_t;
static void model_effect(void *user,kp_effect_t effect,const kp_action_t *action) {
    (void)action;runtime_t *rt=user;
    if(effect==KP_TAP||effect==KP_HOLD){
        assert(kp_runner_start(&rt->runner,&rt->model.config,kp_binding(&rt->model.config,rt->model.triggered_slot),effect==KP_HOLD,rt->model.system));
        ++rt->starts;kp_set_busy(&rt->model,true);
    }else if(effect==KP_RELEASE){++rt->releases;kp_runner_release(&rt->runner,rt->now);}
    else if(effect==KP_CANCEL){++rt->cancels;kp_runner_cancel(&rt->runner);}
}
static void edge(runtime_t *rt,unsigned key,bool down,uint32_t now){rt->now=now;kp_input(&rt->model,key,down,now);}
static void acknowledged(runtime_t *rt,uint32_t now,unsigned modifiers,unsigned key){
    assert(request(&rt->runner,now,modifiers,key));kp_runner_result(&rt->runner,true,now);
    if(!rt->runner.running)kp_set_busy(&rt->model,false);
}
static void integration(void){
    kp_config_t c;kp_defaults(&c);assert(kp_binding_set(&c,0,0,"",NULL,0));
    assert(kp_binding_set(&c,0,1,"",NULL,0));assert(kp_binding_set(&c,0,2,"",NULL,0));
    runtime_t rt={0};kp_init(&rt.model,&c,model_effect,&rt);
    edge(&rt,0,true,0);assert(rt.starts==1&&rt.model.executing);acknowledged(&rt,0,4,0);
    edge(&rt,1,true,10);edge(&rt,1,false,30);assert(rt.starts==1);
    edge(&rt,0,false,50);assert(rt.releases==1);acknowledged(&rt,50,0,0);assert(!rt.model.executing);
    kp_tick(&rt.model,1000);assert(rt.starts==1); // busy input is never replayed
    edge(&rt,0,true,1100);acknowledged(&rt,1100,4,0);
    edge(&rt,2,true,1200);rt.now=1700;kp_tick(&rt.model,1700);
    assert(rt.model.page==KP_MENU&&!rt.runner.running&&rt.cancels==1&&rt.model.blocked);
    edge(&rt,2,false,1710);edge(&rt,0,false,1720);assert(!rt.model.blocked&&rt.releases==1);
    // Releasing a completed multi-step hold must not release another physical key's current hold.
    kp_step_t steps[2]={{{.type=KP_KEYBOARD,.keys={4}},0},{{.type=KP_KEYBOARD,.keys={5}},0}};
    assert(kp_binding_set(&c,0,3,"",steps,2));assert(kp_binding_set(&c,0,4,"",NULL,0));assert(kp_binding_set(&c,0,5,"",NULL,0));
    kp_step_t down={.action={.type=KP_KEYBOARD,.keys={0x51}}};assert(kp_binding_set(&c,0,7,"",&down,1));
    memset(&rt,0,sizeof(rt));kp_init(&rt.model,&c,model_effect,&rt);
    edge(&rt,0,true,0);acknowledged(&rt,0,0,4);acknowledged(&rt,40,0,0);acknowledged(&rt,40,0,5);acknowledged(&rt,80,0,0);
    assert(rt.starts==1&&!rt.runner.running);kp_tick(&rt.model,500);assert(rt.starts==1);
    edge(&rt,1,true,600);acknowledged(&rt,600,0,0x51);edge(&rt,0,false,650);assert(rt.releases==0&&rt.runner.phase==KP_RUN_HOLD);
    edge(&rt,1,false,700);acknowledged(&rt,700,0,0);assert(rt.releases==1&&!rt.runner.running);
}
static void coding_defaults(void) {
    kp_config_t c;kp_defaults(&c);assert(kp_config_valid(&c));
    assert(c.profile_count==1&&c.active_profile==0&&!strcmp(kp_text(&c,c.profiles[0].name),"coding模式"));
    const char *titles[KP_BINDINGS]={"无","撤销","重做","可配置为语音输入","删除","无","无","全选并删除","回车","无","无"};
    for(unsigned system=0;system<KP_SYSTEM_COUNT;system++)for(unsigned slot=0;slot<KP_BINDINGS;slot++) {
        runtime_t rt={0};kp_init(&rt.model,&c,model_effect,&rt);rt.model.system=system;
        unsigned key=slot/4,gesture=slot%4,mod=system==KP_SYSTEM_MACOS?8:1;
        const unsigned expected[][2]={
            {0,0},{mod,29},{system==KP_SYSTEM_MACOS?10:1,system==KP_SYSTEM_MACOS?29:28},
            {4,0},{0,42},{0,0},{0,0},{mod,4},{0,40},{0,0},{0,0}
        };
        unsigned total=slot==7?2:expected[slot][0]||expected[slot][1]?1:0;
        assert(kp_binding_count(&c,kp_binding(&c,slot),system)==total);
        char title[100];kp_binding_title(&c,kp_binding(&c,slot),system,title,sizeof(title));assert(!strcmp(title,titles[slot]));
        // Unbound double/triple slots keep the immediate single-click path.
        if(!total&&slot!=0)continue;
        if(gesture==3) {edge(&rt,key,true,0);rt.now=KP_ACTION_HOLD_MS;kp_tick(&rt.model,rt.now);}
        else {
            for(unsigned n=0;n<=gesture;n++){edge(&rt,key,true,n*50);edge(&rt,key,false,n*50+20);}
            rt.now=400;kp_tick(&rt.model,rt.now);
        }
        assert(rt.starts==(total?1U:0U));
        unsigned reports=0;
        for(unsigned now=400;now<=1000;now++) {
            if(gesture==3&&now==500)edge(&rt,key,false,now);
            kp_action_t action;
            if(!kp_runner_request(&rt.runner,now,&action))continue;
            uint8_t keyboard[8],consumer[2],wanted[8]={0};kp_report(&action,keyboard,consumer);
            if(!(reports%2)) {
                wanted[0]=reports?0:expected[slot][0];wanted[2]=reports?42:expected[slot][1];
                if(slot==7&&reports==2)assert(now>=400+KP_PULSE_MS+KP_DELAY_MS);
            }
            assert(!memcmp(keyboard,wanted,sizeof(wanted))&&!consumer[0]&&!consumer[1]);
            ++reports;kp_runner_result(&rt.runner,true,now);
            if(slot==3&&reports==1)assert(rt.runner.running&&rt.runner.hold);
            if(!rt.runner.running)kp_set_busy(&rt.model,false);
        }
        assert(reports==total*2&&!rt.runner.running&&!rt.runner.failed&&rt.runner.completed==total);
        assert(rt.releases==(gesture==3&&total?1U:0U));
    }
    puts("Coding defaults: gestures, titles and HID reports on macOS/Windows: PASS");
}
int main(void) {
    coding_defaults();
    kp_config_t c;kp_defaults(&c);kp_runner_t r={0};
    kp_step_t steps[2]={{{.type=KP_KEYBOARD,.modifiers=8,.keys={4}},100},{{.type=KP_KEYBOARD,.keys={0x4c}},0}};
    assert(kp_binding_set(&c,0,4,"全选后删除",steps,2));
    const kp_binding_t *b=kp_binding(&c,4);
    assert(kp_runner_start(&r,&c,b,false,KP_SYSTEM_MACOS));assert(!kp_runner_start(&r,&c,b,false,KP_SYSTEM_MACOS));
    assert(request(&r,0,8,4));assert(!request(&r,10,0,0)); // one in-flight report
    kp_runner_result(&r,true,25); // deadlines use acknowledgement time, not scheduling time
    assert(!request(&r,64,0,0));assert(request(&r,65,0,0));kp_runner_result(&r,true,70);
    assert(r.completed==1&&r.running);assert(!request(&r,169,0,0));
    assert(request(&r,170,0,0x4c));kp_runner_result(&r,true,175);
    assert(request(&r,215,0,0));kp_runner_result(&r,true,216);assert(!r.running&&!r.failed&&r.completed==2);
    char label[100];kp_binding_label(&c,b,KP_SYSTEM_MACOS,label,sizeof(label));assert(!strcmp(label,"Command+A → Delete"));
    kp_binding_title(&c,b,KP_SYSTEM_MACOS,label,sizeof(label));assert(!strcmp(label,"全选后删除"));
    // Multi-step holds run once and physical release cannot truncate the macro.
    assert(kp_runner_start(&r,&c,b,true,KP_SYSTEM_MACOS)&&!r.hold);assert(request(&r,0,8,4));kp_runner_result(&r,true,0);
    kp_runner_release(&r,10);assert(!request(&r,39,0,0));assert(request(&r,40,0,0));kp_runner_result(&r,true,40);
    assert(request(&r,140,0,0x4c));kp_runner_result(&r,true,140);assert(request(&r,180,0,0));kp_runner_result(&r,true,180);assert(!r.running);
    // Single-step hold releases only on the physical edge.
    b=kp_binding(&c,3);assert(kp_runner_start(&r,&c,b,true,KP_SYSTEM_MACOS));assert(request(&r,0,4,0));kp_runner_result(&r,true,0);
    assert(!request(&r,10000,0,0));kp_runner_release(&r,10001);assert(request(&r,10001,0,0));kp_runner_result(&r,true,10001);assert(!r.running);
    // Release arriving in the same input batch as the hold becomes a bounded tap.
    assert(kp_runner_start(&r,&c,b,true,KP_SYSTEM_MACOS));kp_runner_release(&r,0);assert(!r.hold);
    assert(request(&r,0,4,0));kp_runner_result(&r,true,0);assert(request(&r,40,0,0));kp_runner_result(&r,true,40);assert(!r.running);
    // Failure and cancellation never replay completed steps.
    b=kp_binding(&c,4);assert(kp_runner_start(&r,&c,b,false,KP_SYSTEM_MACOS));assert(request(&r,0,8,4));kp_runner_result(&r,false,1);
    assert(r.failed&&!r.running&&!request(&r,100,0,0));
    assert(kp_runner_start(&r,&c,b,false,KP_SYSTEM_MACOS));assert(request(&r,0,8,4));kp_runner_result(&r,true,0);kp_runner_cancel(&r);
    assert(!request(&r,1000,0,0));
    // Wrap around and zero delay.
    steps[0].delay_ms=0;assert(kp_binding_set(&c,0,4,"",steps,2));b=kp_binding(&c,4);
    assert(kp_runner_start(&r,&c,b,false,KP_SYSTEM_MACOS));assert(request(&r,UINT32_MAX-20,8,4));kp_runner_result(&r,true,UINT32_MAX-20);
    assert(!request(&r,18,0,0));assert(request(&r,19,0,0));kp_runner_result(&r,true,19);
    assert(request(&r,19,0,0x4c));kp_runner_result(&r,true,19);assert(request(&r,59,0,0));kp_runner_result(&r,true,59);
    assert(!r.running&&r.completed==2);
    // Opposite system branches execute one report pair, with no skipped waits.
    kp_step_t branches[]={
        {.action={.type=KP_KEYBOARD,.modifiers=1,.keys={6},.system=KP_TARGET_WINDOWS},.delay_ms=5000},
        {.action={.type=KP_KEYBOARD,.modifiers=8,.keys={6},.system=KP_TARGET_MACOS},.delay_ms=70},
        {.action={.type=KP_MEDIA,.consumer=233,.system=KP_TARGET_WINDOWS},.delay_ms=5000},
        {.action={.type=KP_KEYBOARD,.keys={4}},.delay_ms=0}
    };
    assert(kp_binding_set(&c,0,0,"",branches,4));
    assert(kp_runner_start(&r,&c,kp_binding(&c,0),false,KP_SYSTEM_MACOS)&&r.total==2);
    assert(request(&r,0,8,6)&&r.index==1);kp_runner_result(&r,true,20);
    assert(request(&r,60,0,0));kp_runner_result(&r,true,65);
    assert(!request(&r,134,0,0));assert(request(&r,135,0,4)&&r.index==3);kp_runner_result(&r,true,135);
    assert(request(&r,175,0,0));kp_runner_result(&r,true,175);assert(!r.running&&r.completed==2);
    assert(kp_runner_start(&r,&c,kp_binding(&c,0),false,KP_SYSTEM_WINDOWS)&&r.total==3);
    assert(request(&r,0,1,6));kp_runner_result(&r,true,0);assert(request(&r,40,0,0));kp_runner_result(&r,true,40);
    assert(!request(&r,5039,0,0));kp_action_t action;
    assert(kp_runner_request(&r,5040,&action)&&action.type==KP_MEDIA&&action.consumer==233&&r.index==2);
    kp_runner_result(&r,true,5040);assert(request(&r,5080,0,0));kp_runner_result(&r,true,5080);
    kp_runner_cancel(&r);assert(!kp_runner_request(&r,10080,&action));
    // Effective single-action holds keep the key down, even with skipped siblings.
    branches[0].delay_ms=80;branches[1].delay_ms=0;
    assert(kp_binding_set(&c,0,1,"",branches,2));
    for(unsigned system=0;system<KP_SYSTEM_COUNT;system++){
        assert(kp_runner_start(&r,&c,kp_binding(&c,1),true,system)&&r.total==1&&r.hold);
        assert(request(&r,0,system==KP_SYSTEM_MACOS?8:1,6));kp_runner_result(&r,true,0);
        assert(!request(&r,5000,0,0));kp_runner_release(&r,5001);assert(request(&r,5001,0,0));
        kp_runner_result(&r,true,5001);assert(!r.running&&r.completed==1);
    }
    branches[0].delay_ms=0;assert(kp_binding_set(&c,0,2,"",branches,1));
    assert(!kp_runner_start(&r,&c,kp_binding(&c,2),false,KP_SYSTEM_MACOS)&&!r.running);
    assert(kp_runner_start(&r,&c,kp_binding(&c,2),false,KP_SYSTEM_WINDOWS));
    assert(request(&r,0,1,6));kp_runner_result(&r,false,1);assert(r.failed&&!r.running);
    integration();
    puts("Keypad macro timing, acknowledgement, hold, cancellation and failure: PASS");
}
