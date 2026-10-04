#include "keypad_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count[KP_SELECT_PROFILE + 1];
static kp_action_t last;
static void effect(void *ctx, kp_effect_t e, const kp_action_t *a) { (void)ctx; ++count[e]; if (a) last = *a; }
static void press(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m,k,true,t); }
static void release(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m,k,false,t); }
static void click(kp_model_t *m, unsigned k, uint32_t t) { press(m,k,t); release(m,k,t+40); kp_tick(m,t+220); }
static void set(kp_config_t *c, unsigned slot, kp_action_t a) {
    kp_step_t step={.action=a}; assert(kp_binding_set(c,0,slot,"","",&step,1));
}
int main(void) {
    kp_config_t c; kp_defaults(&c); assert(kp_config_valid(&c) && sizeof(c)==6144);
    kp_model_t m; kp_init(&m,&c,effect,NULL);
    press(&m,0,0); release(&m,0,50); kp_tick(&m,229); assert(count[KP_TAP]==0);
    kp_tick(&m,230); assert(count[KP_TAP]==1 && last.keys[0]==0x29);
    press(&m,0,300); kp_tick(&m,799); assert(count[KP_HOLD]==0);
    kp_tick(&m,800); assert(count[KP_HOLD]==1 && last.modifiers==4 && !last.keys[0]);
    kp_tick(&m,1200); assert(count[KP_HOLD]==1 && count[KP_RELEASE]==0);
    release(&m,0,1300); assert(count[KP_RELEASE]==1);
    click(&m,1,1700); assert(count[KP_TAP]==2 && last.keys[0]==0x2a);
    press(&m,1,2000); release(&m,1,2040); press(&m,1,2100); release(&m,1,2140);
    assert(count[KP_TAP]==3 && last.modifiers==8 && last.keys[0]==4);
    kp_tick(&m,2400); assert(count[KP_TAP]==3);
    click(&m,2,2500); assert(count[KP_TAP]==4 && last.keys[0]==0x28);
    press(&m,2,2800); kp_tick(&m,3300); assert(m.page==KP_MENU && m.blocked);
    release(&m,2,3400); assert(!m.blocked);
    click(&m,1,3500); click(&m,2,3800); assert(m.page==KP_PROFILES && m.selected==0);
    click(&m,2,4100); assert(count[KP_SELECT_PROFILE]==1 && m.page==KP_PROFILES);
    press(&m,2,4400); kp_tick(&m,4900); release(&m,2,5000); assert(m.page==KP_MENU);
    click(&m,0,5100); click(&m,2,5400); assert(m.page==KP_SETTINGS); // UP wraps to third entry.
    click(&m,2,5700); assert(m.page==KP_HOTSPOT && count[KP_AP_START]==1);
    click(&m,2,6000); assert(m.page==KP_HOTSPOT);
    press(&m,2,6300); kp_tick(&m,6800); release(&m,2,6900);
    assert(m.page==KP_SETTINGS && count[KP_AP_STOP]==1);
    click(&m,1,7000); click(&m,2,7300); assert(m.page==KP_FORGET && !m.selected);
    click(&m,2,7600); assert(m.page==KP_SETTINGS && !count[KP_CLEAR_BOND]);
    click(&m,1,7900); click(&m,2,8200); click(&m,1,8500); click(&m,2,8800);
    assert(count[KP_CLEAR_BOND]==1 && m.page==KP_SETTINGS);
    // Each physical gesture maps independently; OK hold is never a ninth binding.
    for(unsigned slot=0;slot<KP_BINDINGS;slot++) {
        kp_defaults(&c); set(&c,slot,(kp_action_t){.type=KP_KEYBOARD,.keys={4+slot}});
        kp_init(&m,&c,effect,NULL); memset(count,0,sizeof(count)); unsigned key=slot/3,g=slot%3;
        press(&m,key,0);
        if(g==2){kp_tick(&m,499);assert(!count[KP_HOLD]);kp_tick(&m,500);release(&m,key,600);assert(count[KP_HOLD]==1&&count[KP_RELEASE]==1);}
        else {release(&m,key,40);if(g==1){press(&m,key,100);release(&m,key,140);}kp_tick(&m,400);assert(count[KP_TAP]==1);}
        assert(last.keys[0]==4+slot && m.triggered_slot==slot);
    }
    // A second press held to 500ms becomes a hold, never a double tap.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    press(&m,0,0);release(&m,0,30);press(&m,0,100);kp_tick(&m,600);release(&m,0,700);kp_tick(&m,1000);
    assert(!count[KP_TAP] && count[KP_HOLD]==1);
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    press(&m,0,UINT32_MAX-200);kp_tick(&m,299);assert(count[KP_HOLD]==1);release(&m,0,350);
    // Link/page cancellation drops pending gestures and waits for physical release.
    press(&m,0,500);release(&m,0,550);kp_cancel(&m);kp_tick(&m,900);assert(!count[KP_TAP]);
    press(&m,0,1000);kp_cancel(&m);kp_tick(&m,1700);assert(m.blocked);release(&m,0,1800);
    click(&m,0,1900);assert(count[KP_TAP]==1);
    // Gestures begun while a macro runs remain ignored after it finishes.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));kp_set_busy(&m,true);
    press(&m,1,0);kp_set_busy(&m,false);kp_tick(&m,600);release(&m,1,700);kp_tick(&m,1000);
    assert(!count[KP_TAP] && !count[KP_HOLD] && !count[KP_RELEASE]);
    kp_set_busy(&m,true);press(&m,2,1100);kp_tick(&m,1600);assert(m.page==KP_MENU&&!m.executing&&count[KP_CANCEL]);release(&m,2,1700);
    // Feedback expires at exactly 500ms, including wrap; menus cannot be timed out.
    kp_init(&m,&c,effect,NULL);m.feedback=(kp_feedback_t){.visible=true,.until=300};
    kp_tick(&m,299);assert(m.feedback.visible);kp_tick(&m,300);assert(!m.feedback.visible);
    m.feedback.visible=true;m.feedback.until=100;kp_tick(&m,UINT32_MAX-50);assert(m.feedback.visible);
    kp_tick(&m,100);assert(!m.feedback.visible);
    m.feedback.visible=true;m.executing=true;kp_tick(&m,1000);assert(m.feedback.visible);
    kp_cancel(&m);assert(!m.feedback.visible);
    // Recovery/disconnection keeps the actual failed result for its 500ms window.
    m.feedback=(kp_feedback_t){.visible=true,.completed=1,.total=2,.state=KP_FEEDBACK_RUNNING};m.executing=true;
    kp_link_changed(&m,UINT32_MAX-100);assert(!m.executing&&m.feedback.visible&&m.feedback.completed==1&&m.feedback.state==KP_FEEDBACK_FAILED);
    uint32_t until=m.feedback.until;kp_link_changed(&m,100);assert(m.feedback.visible&&m.feedback.until==until);
    kp_tick(&m,until);assert(!m.feedback.visible);
    m.page=KP_MENU;m.feedback.visible=true;kp_link_changed(&m,200);assert(!m.feedback.visible);
    // HID validation still preserves six-key rollover and both modifier sides.
    uint8_t keys[8],media[2];kp_action_t a={.type=KP_KEYBOARD,.modifiers=128};assert(kp_action_valid(&a,false));
    kp_report(&a,keys,media);assert(keys[0]==128&&keys[2]==0);
    a=(kp_action_t){.type=KP_MEDIA,.consumer=0xe9};kp_report(&a,keys,media);assert(!keys[0]&&media[0]==0xe9);
    kp_report(NULL,keys,media);for(unsigned i=0;i<8;i++)assert(!keys[i]);assert(!media[0]);
    a=(kp_action_t){.type=KP_KEYBOARD,.keys={0x66}};assert(!kp_action_valid(&a,false));
    a.keys[0]=4;a.keys[1]=4;assert(!kp_action_valid(&a,false));
    char text[80];a=(kp_action_t){.type=KP_MEDIA,.consumer=0xb5};kp_action_label(&a,text,sizeof(text));assert(!strcmp(text,"下一首"));
    a.consumer=0xb6;kp_action_label(&a,text,sizeof(text));assert(!strcmp(text,"上一首"));
    assert(kp_text_valid("编程浏览龘，ABC",16,false));assert(!kp_text_valid("😀",16,false));
    assert(!kp_text_valid("\xc0\xaf",16,false));assert(!kp_text_valid("\xed\xa0\x80",16,false));
    assert(!kp_text_valid("a\nb",16,false)&&kp_text_valid("a\nb",64,true));
    assert(!kp_text_valid("12345678901234567",16,false));
    puts("Keypad gestures, navigation, busy input and feedback: PASS");
}
