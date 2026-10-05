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
static void quick(kp_model_t *m, unsigned k, uint32_t t) { press(m,k,t); release(m,k,t+20); }
static void set(kp_config_t *c, unsigned slot, kp_action_t a) {
    kp_step_t step={.action=a}; assert(kp_binding_set(c,0,slot,"",&step,1));
}
int main(void) {
    kp_config_t c; kp_defaults(&c); assert(kp_config_valid(&c) && sizeof(c)==6144);
    set(&c,1,(kp_action_t){.type=KP_KEYBOARD,.keys={5}});
    kp_model_t m; kp_init(&m,&c,effect,NULL);
    press(&m,0,0); release(&m,0,50); kp_tick(&m,229); assert(count[KP_TAP]==0);
    kp_tick(&m,230); assert(count[KP_TAP]==1 && last.keys[0]==0x29);
    press(&m,0,300); kp_tick(&m,499); assert(count[KP_HOLD]==0);
    kp_tick(&m,500); assert(count[KP_HOLD]==1 && last.modifiers==4 && !last.keys[0]);
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
    quick(&m,2,5700); assert(m.page==KP_AP_CONFIRM && !m.selected && !count[KP_AP_START]);
    quick(&m,2,5800); assert(m.page==KP_SETTINGS && !count[KP_AP_START] && !count[KP_AP_STOP]);
    quick(&m,2,5900); assert(m.page==KP_AP_CONFIRM);
    press(&m,2,6000); kp_tick(&m,6499); assert(m.page==KP_AP_CONFIRM);
    kp_tick(&m,6500);release(&m,2,6510);assert(m.page==KP_SETTINGS&&!count[KP_AP_START]&&!count[KP_AP_STOP]);
    quick(&m,2,6600);quick(&m,0,6700);assert(m.selected==1);quick(&m,1,6800);assert(m.selected==0);
    quick(&m,1,6900);quick(&m,2,7000);assert(m.page==KP_HOTSPOT&&count[KP_AP_START]==1);
    quick(&m,2,7100); assert(m.page==KP_HOTSPOT);
    press(&m,2,7200); kp_tick(&m,7700); release(&m,2,7800);
    assert(m.page==KP_SETTINGS && count[KP_AP_STOP]==1);
    quick(&m,1,7900); quick(&m,2,8000); assert(m.page==KP_FORGET && !m.selected);
    quick(&m,2,8100); assert(m.page==KP_SETTINGS && !count[KP_CLEAR_BOND]);
    quick(&m,1,8200); quick(&m,2,8300); quick(&m,1,8400); quick(&m,2,8500);
    assert(count[KP_CLEAR_BOND]==1 && m.page==KP_SETTINGS);
    // Each physical gesture maps independently; OK hold remains outside the configurable slots.
    for(unsigned slot=0;slot<KP_BINDINGS;slot++) {
        kp_defaults(&c); set(&c,slot,(kp_action_t){.type=KP_KEYBOARD,.keys={4+slot}});
        kp_init(&m,&c,effect,NULL); memset(count,0,sizeof(count)); unsigned key=slot/4,g=slot%4;
        press(&m,key,0);
        if(g==3){kp_tick(&m,199);assert(!count[KP_HOLD]);kp_tick(&m,200);release(&m,key,600);assert(count[KP_HOLD]==1&&count[KP_RELEASE]==1);}
        else {release(&m,key,40);if(g>=1){press(&m,key,100);release(&m,key,140);}if(g==2){assert(!count[KP_TAP]);press(&m,key,200);assert(!count[KP_TAP]);release(&m,key,240);}kp_tick(&m,400);assert(count[KP_TAP]==1);}
        assert(last.keys[0]==4+slot && m.triggered_slot==slot);
    }
    // A second press held to 200ms becomes a hold, never a double tap.
    set(&c,1,(kp_action_t){.type=KP_KEYBOARD,.keys={5}});
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
    // No double binding: every release sends the single immediately, even with a title on an empty action.
    kp_defaults(&c);assert(kp_binding_set(&c,0,1,"空双击",NULL,0));kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    for(unsigned i=0;i<6;i++){quick(&m,0,i*50);assert(count[KP_TAP]==i+1&&!m.key[0].pending);}
    kp_tick(&m,1000);assert(count[KP_TAP]==6);
    // Bound singles/doubles: five quick taps yield two doubles and one delayed single.
    set(&c,1,(kp_action_t){.type=KP_KEYBOARD,.keys={5}});kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    for(unsigned i=0;i<5;i++){quick(&m,0,i*50);assert(count[KP_TAP]==(i+1)/2);if(i%2)assert(m.triggered_slot==1&&last.keys[0]==5);}
    kp_tick(&m,399);assert(count[KP_TAP]==2);kp_tick(&m,400);assert(count[KP_TAP]==3&&m.triggered_slot==0);
    // At exactly the click deadline, the previous single expires before the next press.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));quick(&m,0,UINT32_MAX-99);
    kp_tick(&m,99);assert(!count[KP_TAP]);press(&m,0,100);assert(count[KP_TAP]==1);release(&m,0,120);kp_tick(&m,300);assert(count[KP_TAP]==2);
    // 199ms is a short press; 200ms starts a hold. OK remains a 500ms system gesture.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));press(&m,0,0);release(&m,0,199);kp_tick(&m,379);assert(count[KP_TAP]==1&&!count[KP_HOLD]);
    press(&m,0,400);release(&m,0,600);assert(count[KP_HOLD]==1&&count[KP_RELEASE]==1);kp_tick(&m,1000);assert(count[KP_TAP]==1);
    press(&m,2,1100);kp_tick(&m,1599);assert(m.page==KP_CONSOLE);kp_tick(&m,1600);assert(m.page==KP_MENU);release(&m,2,1610);
    // Hold-only UP starts on the press, stays once, and releases on the edge.
    assert(kp_binding_set(&c,0,0,"空单击",NULL,0));assert(kp_binding_set(&c,0,1,"",NULL,0));
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));press(&m,0,UINT32_MAX-50);assert(count[KP_HOLD]==1);
    kp_tick(&m,500);assert(count[KP_HOLD]==1);release(&m,0,501);assert(count[KP_RELEASE]==1&&!count[KP_TAP]);
    // Inputs begun while busy, including hold-only keys, cannot be replayed after completion.
    kp_set_busy(&m,true);quick(&m,0,600);quick(&m,1,650);kp_set_busy(&m,false);kp_tick(&m,1000);assert(!count[KP_TAP]&&count[KP_HOLD]==1);
    press(&m,0,1100);kp_cancel(&m);assert(m.blocked&&count[KP_CANCEL]);release(&m,0,1200);assert(!m.blocked&&count[KP_RELEASE]==1);
    // Bound triples suppress both single and double until their respective windows expire.
    for(unsigned key=0;key<3;key++) {
        kp_defaults(&c);set(&c,key*4+1,(kp_action_t){.type=KP_KEYBOARD,.keys={5}});
        set(&c,key*4+2,(kp_action_t){.type=KP_KEYBOARD,.keys={6}});
        kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
        quick(&m,key,0);kp_tick(&m,199);assert(!count[KP_TAP]);kp_tick(&m,200);assert(count[KP_TAP]==1&&m.triggered_slot==key*4);
        quick(&m,key,300);quick(&m,key,350);kp_tick(&m,549);assert(count[KP_TAP]==1);
        kp_tick(&m,550);assert(count[KP_TAP]==2&&m.triggered_slot==key*4+1&&last.keys[0]==5);
        quick(&m,key,600);quick(&m,key,650);press(&m,key,829);assert(count[KP_TAP]==2);
        kp_tick(&m,849);assert(count[KP_TAP]==2);release(&m,key,850);assert(count[KP_TAP]==3&&m.triggered_slot==key*4+2&&last.keys[0]==6);
        kp_tick(&m,1100);assert(count[KP_TAP]==3);
        // An exact 180ms gap expires the double before a third press.
        quick(&m,key,1200);quick(&m,key,1250);press(&m,key,1450);assert(count[KP_TAP]==4&&m.triggered_slot==key*4+1);
        release(&m,key,1470);kp_tick(&m,1650);assert(count[KP_TAP]==5&&m.triggered_slot==key*4);
        // Duplicate physical edges do not count as extra clicks.
        press(&m,key,1700);press(&m,key,1710);release(&m,key,1720);release(&m,key,1730);
        kp_tick(&m,1900);assert(count[KP_TAP]==6&&m.triggered_slot==key*4);
        // A third hold cancels all short clicks; OK always navigates at 500ms.
        quick(&m,key,2000);quick(&m,key,2050);press(&m,key,2100);
        unsigned threshold=key<2?200:500;kp_tick(&m,2100+threshold-1);assert(count[KP_TAP]==6);
        kp_tick(&m,2100+threshold);release(&m,key,2700);kp_tick(&m,3000);
        assert(count[KP_TAP]==6);if(key<2)assert(count[KP_HOLD]==1);else assert(m.page==KP_MENU);
    }
    // Triple-only shortcuts still defer singles; an unbound double emits nothing.
    kp_defaults(&c);set(&c,2,(kp_action_t){.type=KP_KEYBOARD,.keys={6}});
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));quick(&m,0,0);quick(&m,0,50);
    kp_tick(&m,250);assert(!count[KP_TAP]);
    for(unsigned i=0;i<7;i++)quick(&m,0,300+i*50);
    assert(count[KP_TAP]==2&&m.triggered_slot==2);kp_tick(&m,800);assert(count[KP_TAP]==3&&m.triggered_slot==0);
    // Both first and second pending clicks are dropped on disconnect, busy and page change.
    for(unsigned n=1;n<=2;n++)for(unsigned cancel=0;cancel<3;cancel++) {
        kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
        if(cancel==2)press(&m,2,0);
        for(unsigned i=0;i<n;i++)quick(&m,0,(cancel==2?350:0)+i*50);
        if(cancel==0)kp_link_changed(&m,100);else if(cancel==1){kp_set_busy(&m,true);kp_set_busy(&m,false);}
        else {kp_tick(&m,500);release(&m,2,510);assert(m.page==KP_MENU);}
        kp_tick(&m,1000);assert(!count[KP_TAP]);
    }
    // A triple window across UINT32_MAX uses elapsed time, without premature dispatch.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    quick(&m,0,UINT32_MAX-150);quick(&m,0,UINT32_MAX-100);press(&m,0,90);
    assert(!count[KP_TAP]);release(&m,0,110);assert(count[KP_TAP]==1&&m.triggered_slot==2);
    // Triple mapping prevents the hold-only press shortcut, even if single/double are empty.
    assert(kp_binding_set(&c,0,0,"",NULL,0));kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    press(&m,0,0);assert(!count[KP_HOLD]);kp_tick(&m,200);assert(count[KP_HOLD]==1);release(&m,0,220);
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
    assert(kp_text_valid("编程浏览，ABC",16,false));assert(!kp_text_valid("😀",16,false));
    assert(!kp_text_valid("龘",16,false)&&!kp_codepoint_supported(0x9f98));
    assert(kp_text_utf8_valid("编程龘",16,false));
    assert(!kp_text_utf8_valid("\xc0\xaf",16,false)&&!kp_text_utf8_valid("\xed\xa0\x80",16,false));
    assert(!kp_text_utf8_valid("a\nb",16,false)&&kp_text_utf8_valid("a\nb",64,true));
    assert(!kp_text_utf8_valid("\xc2\x85",16,false)&&!kp_text_utf8_valid("12345678901234567",16,false));
    assert(!kp_text_valid("\xc0\xaf",16,false));assert(!kp_text_valid("\xed\xa0\x80",16,false));
    assert(!kp_text_valid("a\nb",16,false)&&kp_text_valid("a\nb",64,true));
    assert(!kp_text_valid("12345678901234567",16,false));
    puts("Keypad gestures, navigation, busy input and feedback: PASS");
}
