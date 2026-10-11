#include "keypad_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count[KP_WIFI_START + 1];
static kp_action_t last;
static void effect(void *ctx, kp_effect_t e, const kp_action_t *a) { (void)ctx; ++count[e]; if (a) last = *a; }
static void press(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m,k,true,t); }
static void release(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m,k,false,t); }
static void click(kp_model_t *m, unsigned k, uint32_t t) { press(m,k,t); release(m,k,t+40); kp_tick(m,t+220); }
static void quick(kp_model_t *m, unsigned k, uint32_t t) { press(m,k,t); release(m,k,t+20); }
static void set(kp_config_t *c, unsigned slot, kp_action_t a) {
    kp_step_t step={.action=a}; assert(kp_binding_set(c,0,slot,"",&step,1));
}
// Gesture timing tests use explicit bindings, independent of the shipped scenario.
static void gesture_config(kp_config_t *c) {
    kp_config_empty(c); assert(kp_profile_add(c,"Gestures"));
    set(c,0,(kp_action_t){.type=KP_KEYBOARD,.keys={0x29}});
    set(c,3,(kp_action_t){.type=KP_KEYBOARD,.modifiers=4});
    set(c,4,(kp_action_t){.type=KP_KEYBOARD,.keys={0x2a}});
    set(c,5,(kp_action_t){.type=KP_KEYBOARD,.modifiers=8,.keys={4}});
    set(c,7,(kp_action_t){.type=KP_KEYBOARD,.keys={0x51}});
    set(c,8,(kp_action_t){.type=KP_KEYBOARD,.keys={0x28}});
}
static void link_navigation(void) {
    kp_config_t c;gesture_config(&c);kp_model_t m;
    // Connection churn cannot restart the global OK hold deadline.
    for(unsigned other=0;other<2;other++) {
        kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
        if(other)press(&m,0,0);
        press(&m,2,10);
        for(unsigned now=60;now<510;now+=50){kp_link_changed(&m,now);kp_tick(&m,now);}
        kp_tick(&m,509);assert(m.page==KP_CONSOLE);
        kp_tick(&m,510);assert(m.page==KP_MENU&&!count[KP_TAP]&&!count[KP_HOLD]);
        release(&m,2,520);if(other)release(&m,0,530);assert(!m.blocked);
    }
    // Local menu presses survive a connection change between their edges.
    for(unsigned key=0;key<3;key++) {
        kp_init(&m,&c,effect,NULL);m.page=KP_MENU;memset(count,0,sizeof(count));
        press(&m,key,0);kp_link_changed(&m,10);release(&m,key,20);
        assert(key==2?m.page==KP_CONSOLE:m.page==KP_MENU&&m.selected==(key==0?2U:1U));
        assert(!count[KP_TAP]&&!count[KP_HOLD]);
    }
    // The same preserved OK press must never replay its keyboard tap.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    press(&m,2,0);kp_link_changed(&m,10);release(&m,2,20);kp_tick(&m,300);
    assert(m.page==KP_CONSOLE&&!count[KP_TAP]);
    press(&m,0,400);kp_link_changed(&m,410);release(&m,0,420);kp_tick(&m,700);
    assert(!count[KP_TAP]&&!count[KP_HOLD]);click(&m,0,800);assert(count[KP_TAP]==1);
    // A wake-only press remains wake-only through reconnection.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));kp_tick(&m,30000);
    press(&m,2,30100);kp_link_changed(&m,30200);kp_tick(&m,31000);release(&m,2,31010);
    assert(m.page==KP_CONSOLE&&!count[KP_TAP]);
    press(&m,2,31100);kp_link_changed(&m,31200);kp_tick(&m,31600);assert(m.page==KP_MENU);
    // Preserve elapsed hold time across the millisecond counter wrap.
    kp_init(&m,&c,effect,NULL);press(&m,2,UINT32_MAX-200);
    kp_link_changed(&m,UINT32_MAX-100);kp_link_changed(&m,100);kp_tick(&m,299);
    assert(m.page==KP_MENU);
    memset(count,0,sizeof(count));puts("BLE churn preserves local navigation and OK hold, cancels HID input, and respects wake filtering: PASS");
}
int main(void) {
    link_navigation();
    kp_config_t c; gesture_config(&c); assert(kp_config_valid(&c) && sizeof(c)==6144);
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
    m.selected=KP_SETTING_PASSPORT;quick(&m,2,5500);assert(m.page==KP_LAN);
    quick(&m,2,5550);assert(m.page==KP_HOTSPOT&&!m.lan.wifi&&m.lan.state==KP_AP_STARTING&&count[KP_AP_START]==1);
    quick(&m,2,5600);assert(m.page==KP_HOTSPOT&&count[KP_AP_START]==1);
    press(&m,2,5700);kp_tick(&m,6200);release(&m,2,6210);assert(m.page==KP_LAN&&!m.selected&&count[KP_AP_STOP]==1);
    quick(&m,2,6250);assert(m.page==KP_HOTSPOT&&count[KP_AP_START]==2);
    quick(&m,2,6400);assert(m.page==KP_HOTSPOT&&count[KP_AP_START]==2);
    press(&m,2,6450);kp_tick(&m,6950);release(&m,2,6960);assert(m.page==KP_LAN&&count[KP_AP_STOP]==2);
    quick(&m,1,7000);quick(&m,2,7050);assert(m.page==KP_HOTSPOT&&m.lan.wifi&&count[KP_WIFI_START]==1);
    m.lan.state=KP_AP_NO_WIFI;quick(&m,2,7100);assert(m.page==KP_HOTSPOT&&count[KP_WIFI_START]==1);
    kp_hotspot_exit(&m);assert(m.page==KP_LAN&&m.selected==1&&count[KP_AP_STOP]==3);
    m.lan.configured=true;quick(&m,2,7200);assert(m.page==KP_HOTSPOT&&count[KP_WIFI_START]==2);
    kp_hotspot_exit(&m);assert(m.page==KP_LAN&&m.selected==1&&count[KP_AP_STOP]==4);
    press(&m,2,7250);kp_tick(&m,7750);release(&m,2,7760);assert(m.page==KP_SETTINGS&&m.selected==KP_SETTING_PASSPORT);
    quick(&m,1,7800);quick(&m,2,7900);assert(m.page==KP_FORGET&&!m.selected);
    quick(&m,2,7950);assert(m.page==KP_SETTINGS&&!count[KP_CLEAR_BOND]);
    quick(&m,0,8000);quick(&m,2,8050);quick(&m,1,8100);quick(&m,2,8150);
    assert(count[KP_CLEAR_BOND]==1&&m.page==KP_SETTINGS);
    // Each physical gesture maps independently; OK hold remains outside the configurable slots.
    for(unsigned slot=0;slot<KP_BINDINGS;slot++) {
        gesture_config(&c); set(&c,slot,(kp_action_t){.type=KP_KEYBOARD,.keys={4+slot}});
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
    gesture_config(&c);assert(kp_binding_set(&c,0,1,"空双击",NULL,0));kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
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
        gesture_config(&c);set(&c,key*4+1,(kp_action_t){.type=KP_KEYBOARD,.keys={5}});
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
    gesture_config(&c);set(&c,2,(kp_action_t){.type=KP_KEYBOARD,.keys={6}});
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
    // Screen edit previews are drafts; cancel/failure never replaces committed settings.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));m.page=KP_SETTINGS;
    quick(&m,2,0);assert(m.page==KP_SCREEN);quick(&m,2,50);assert(m.page==KP_SCREEN_EDIT);
    quick(&m,0,100);assert(m.screen_draft.brightness==85&&m.screen_config.brightness==80&&kp_screen_level(&m)==85);
    press(&m,2,200);kp_tick(&m,700);release(&m,2,710);
    assert(m.page==KP_SCREEN&&m.screen_config.brightness==80&&!count[KP_SAVE_SCREEN]);
    quick(&m,2,800);quick(&m,0,850);quick(&m,2,900);
    assert(m.screen_saving&&count[KP_SAVE_SCREEN]==1);
    quick(&m,0,950);assert(m.screen_draft.brightness==85); // Cannot edit during a commit.
    kp_screen_saved(&m,false);assert(m.page==KP_SCREEN_EDIT&&m.screen_save_failed&&m.screen_config.brightness==80);
    quick(&m,2,1000);kp_screen_saved(&m,true);assert(m.page==KP_SCREEN&&m.screen_config.brightness==85);
    quick(&m,1,1100);quick(&m,1,1150);quick(&m,2,1200);assert(m.screen_setting==KP_SCREEN_DIM_BRIGHTNESS);
    quick(&m,1,1250);assert(kp_screen_level(&m)==15&&m.screen_config.dim_brightness==20);
    kp_tick(&m,300000);assert(m.screen.state==KP_SCREEN_ACTIVE); // Editing stays visible.
    // Sleeping on a hold-only shortcut or long OK consumes the entire first gesture.
    gesture_config(&c);assert(kp_binding_set(&c,0,0,"",NULL,0));
    for(unsigned key=0;key<3;key++){
        kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));kp_tick(&m,30000);
        assert(m.screen.state==KP_SCREEN_OFF);press(&m,key,30100);kp_tick(&m,32000);release(&m,key,32100);
        assert(m.page==KP_CONSOLE&&!count[KP_TAP]&&!count[KP_HOLD]&&!count[KP_RELEASE]&&!m.blocked);
        quick(&m,key,32200);kp_tick(&m,32500);
        assert(count[KP_TAP]+count[KP_HOLD]==1);
    }
    // Background link transitions preserve the idle deadline and current page.
    kp_init(&m,&c,effect,NULL);kp_tick(&m,10000);kp_link_changed(&m,20000);kp_tick(&m,30000);
    assert(m.screen.state==KP_SCREEN_OFF&&m.page==KP_CONSOLE);
    // Real A/B records, mandatory first confirmation and repeatable edits.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    uint8_t identities[2][7]={{0,1,2,3,4,5,6},{1,6,5,4,3,2,1}};
    assert(kp_devices_reconcile(&m.devices,identities,2)==1);
    m.devices.slot[0].system=KP_SYSTEM_MACOS;m.devices.target=1;
    m.actual_device=1;m.hid_ready=m.devices_committed=true;m.system=KP_SYSTEM_UNSET;
    assert(!kp_model_can_send(&m));kp_system_open(&m,1);
    assert(m.page==KP_SYSTEM&&m.selected==0&&!kp_model_can_send(&m));
    quick(&m,1,0);assert(m.selected==1&&m.devices.slot[1].system==KP_SYSTEM_UNSET);
    quick(&m,2,50);assert(m.system_saving&&count[KP_SAVE_SYSTEM]==1);
    press(&m,2,100);kp_tick(&m,600);release(&m,2,610);assert(m.page==KP_SYSTEM);
    kp_system_saved(&m,false);assert(!kp_model_can_send(&m)&&m.devices.slot[1].system==KP_SYSTEM_UNSET);
    press(&m,2,700);kp_tick(&m,1200);release(&m,2,1210);
    assert(m.page==KP_DEVICE_ACTIONS&&!kp_model_can_send(&m));
    quick(&m,2,1300);assert(m.page==KP_SYSTEM);quick(&m,1,1350);quick(&m,2,1400);
    kp_system_saved(&m,true);assert(m.page==KP_CONSOLE&&m.system==KP_SYSTEM_WINDOWS&&kp_model_can_send(&m));
    assert(m.devices.slot[0].system==KP_SYSTEM_MACOS);
    for(unsigned slot=0;slot<2;slot++)for(unsigned repeat=0;repeat<3;repeat++){
        uint8_t original=m.devices.slot[slot].system,other=m.devices.slot[1-slot].system;
        uint32_t at=2000+slot*10000+repeat*2000;
        kp_system_open(&m,slot);quick(&m,1,at);assert(!kp_model_can_send(&m));
        press(&m,2,at+50);kp_tick(&m,at+550);release(&m,2,at+560);
        assert(kp_model_can_send(&m)&&m.devices.slot[slot].system==original);
        kp_system_open(&m,slot);quick(&m,1,at+600);quick(&m,2,at+650);kp_system_saved(&m,false);
        assert(!kp_model_can_send(&m)&&m.devices.slot[slot].system==original);
        quick(&m,2,at+700);kp_system_saved(&m,true);
        assert(m.page==KP_CONSOLE&&kp_model_can_send(&m)&&m.devices.slot[slot].system==1-original&&m.devices.slot[1-slot].system==other);
    }
    m.devices.target=0;assert(!kp_model_can_send(&m));m.actual_device=0;assert(kp_model_can_send(&m));
    m.hid_ready=false;assert(!kp_model_can_send(&m));m.hid_ready=true;m.devices_committed=false;assert(!kp_model_can_send(&m));
    m.devices_committed=true;m.device_selected=0;m.page=KP_DEVICE_ACTIONS;m.selected=0;
    quick(&m,2,30000);assert(count[KP_SWITCH_DEVICE]==1&&m.devices.target==0);
    kp_device_switched(&m,false);assert(m.page==KP_DEVICE_ACTIONS&&m.system_save_failed);
    kp_device_switched(&m,true);assert(m.page==KP_CONSOLE&&m.selected==0&&!m.system_save_failed);
    // Opening control does not grant input to an offline or unconfigured peer.
    m.hid_ready=false;assert(!kp_model_can_send(&m));m.hid_ready=true;
    m.devices.slot[0].system=KP_SYSTEM_UNSET;assert(!kp_model_can_send(&m));
    m.page=KP_DEVICES;m.selected=1;quick(&m,2,30100);assert(m.page==KP_DEVICE_ACTIONS&&m.device_selected==1&&m.selected==0);
    kp_devices_defaults(&m.devices);m.page=KP_DEVICES;m.selected=1;quick(&m,2,30200);
    assert(m.page==KP_ADD_DEVICE&&count[KP_ADD_PAIRING]==1);
    press(&m,2,30300);kp_tick(&m,30800);release(&m,2,30810);assert(m.page==KP_DEVICES&&count[KP_END_PAIRING]==1);
    // Reordering and RPA changes cannot reassign identity metadata. Removal
    // clears only that slot, and a replacement never inherits its name/OS.
    kp_devices_t devices;kp_devices_defaults(&devices);kp_devices_reconcile(&devices,identities,2);
    devices.slot[0].system=0;devices.slot[1].system=1;devices.target=1;
    assert(kp_device_name_set(&devices.slot[0],"MacBook Air"));
    assert(!kp_device_name_set(&devices.slot[1],"😀")&&!kp_device_name_set(&devices.slot[1],""));
    uint8_t reversed[2][7];memcpy(reversed[0],identities[1],7);memcpy(reversed[1],identities[0],7);
    assert(kp_devices_reconcile(&devices,reversed,2)==-1&&devices.slot[0].system==0&&devices.slot[1].system==1);
    kp_devices_reconcile(&devices,reversed,1);assert(!devices.slot[0].occupied&&!devices.slot[0].name[0]&&devices.slot[0].system==KP_SYSTEM_UNSET);
    reversed[1][1]=99;assert(kp_devices_reconcile(&devices,reversed,2)==0&&devices.slot[0].system==KP_SYSTEM_UNSET);
    assert(!strcmp(kp_device_name(&devices,0),"设备 A"));assert(kp_devices_valid(&devices));
    // Fixed modifiers keep identical HID codes on both host systems.
    for(unsigned system=0;system<KP_SYSTEM_COUNT;system++)for(unsigned mods=0;mods<256;mods++){
        kp_action_t source={.type=KP_KEYBOARD,.modifiers=mods,.keys={4}},resolved;
        assert(kp_action_valid(&source,false)&&kp_action_resolve(&source,system,&resolved));
        kp_report(&resolved,keys,media);assert(keys[0]==mods&&keys[2]==4&&!media[0]);
        assert(source.modifiers==mods);
    }
    a=(kp_action_t){.type=KP_KEYBOARD,.modifiers=0x44};
    kp_action_label(&a,text,sizeof(text));assert(!strcmp(text,"Option/Alt+ROption/Alt"));
    for(unsigned target=0;target<KP_TARGET_COUNT;target++)for(unsigned system=0;system<KP_SYSTEM_COUNT;system++){
        a.system=target;assert(kp_action_valid(&a,false));
        assert(kp_action_resolve(&a,system,&last)==(!target||target==(system==KP_SYSTEM_WINDOWS?KP_TARGET_WINDOWS:KP_TARGET_MACOS)));
    }
    a.system=3;assert(!kp_action_valid(&a,false));assert(!kp_action_resolve(&a,KP_SYSTEM_COUNT,&last));
    // Five settings scroll through four visible rows; runtime state survives Back.
    kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));m.page=KP_SETTINGS;
    for(unsigned i=0;i<KP_SETTINGS_COUNT;i++){assert(m.selected==i);assert(kp_settings_first(i)==(i==4?1U:0U));quick(&m,1,i*50);}
    assert(m.selected==0);quick(&m,1,300);quick(&m,1,325);quick(&m,2,350);assert(m.page==KP_TRANSFER&&m.selected==1&&!m.transfer.enabled);
    quick(&m,2,400);assert(count[KP_TRANSFER_START]==1&&!m.transfer.enabled);
    m.transfer.enabled=true;m.transfer.state=KP_TRANSFER_CONNECTING;
    quick(&m,0,450);quick(&m,2,500);assert(count[KP_TRANSFER_STOP]==1);
    m.transfer.state=KP_TRANSFER_STOPPING;quick(&m,2,550);quick(&m,1,600);assert(count[KP_TRANSFER_STOP]==1&&m.selected==0);
    press(&m,2,650);kp_tick(&m,1150);release(&m,2,1160);assert(m.page==KP_SETTINGS&&m.selected==2&&m.transfer.enabled);
    quick(&m,2,1200);assert(m.page==KP_TRANSFER&&m.selected==0);m.transfer.state=KP_TRANSFER_FAILED;
    quick(&m,1,1250);quick(&m,2,1300);assert(count[KP_TRANSFER_START]==2);
    const kp_page_t setting_pages[]={KP_SCREEN,KP_DEVICES,KP_TRANSFER,KP_LAN,KP_FORGET};
    for(unsigned item=0;item<KP_SETTINGS_COUNT;item++){
        kp_init(&m,&c,effect,NULL);m.page=KP_SETTINGS;m.selected=item;
        quick(&m,2,0);assert(m.page==setting_pages[item]);
        press(&m,2,50);kp_tick(&m,550);release(&m,2,560);
        assert(m.page==KP_SETTINGS&&m.selected==(item==KP_SETTING_FORGET?0:item));
    }
    // Device labels and fallback titles reflect the actual execution path.
    kp_step_t display_steps[]={
        {.action={.type=KP_KEYBOARD,.modifiers=10,.keys={29},.system=KP_TARGET_MACOS},.delay_ms=80},
        {.action={.type=KP_KEYBOARD,.modifiers=1,.keys={28},.system=KP_TARGET_WINDOWS}}
    };
    gesture_config(&c);assert(kp_binding_set(&c,0,10,"",display_steps,2));
    const kp_binding_t *display_binding=kp_binding(&c,10);char label[200];
    for(unsigned system=0;system<KP_SYSTEM_COUNT;system++){
        const char *expected=system==KP_SYSTEM_MACOS?"Shift+Command+Z":"Control+Y";
        assert(kp_binding_count(&c,display_binding,system)==1);
        kp_binding_label(&c,display_binding,system,label,sizeof(label));assert(!strcmp(label,expected));
        kp_binding_title(&c,display_binding,system,label,sizeof(label));assert(!strcmp(label,expected));
    }
    display_steps[1].action.system=KP_TARGET_MACOS;
    assert(kp_binding_set(&c,0,10,"",display_steps,2));display_binding=kp_binding(&c,10);
    kp_binding_label(&c,display_binding,KP_SYSTEM_WINDOWS,label,sizeof(label));assert(!strcmp(label,"无"));
    kp_binding_title(&c,display_binding,KP_SYSTEM_WINDOWS,label,sizeof(label));assert(!strcmp(label,"无"));
    kp_binding_title(&c,display_binding,KP_SYSTEM_MACOS,label,sizeof(label));assert(!strcmp(label,"组合指令"));
    display_steps[1].action=(kp_action_t){.type=KP_MEDIA,.consumer=205};
    assert(kp_binding_set(&c,0,10,"",display_steps,2));display_binding=kp_binding(&c,10);
    kp_binding_label(&c,display_binding,KP_SYSTEM_WINDOWS,label,sizeof(label));assert(!strcmp(label,"播放暂停"));
    kp_binding_label(&c,display_binding,KP_SYSTEM_MACOS,label,sizeof(label));assert(!strcmp(label,"Shift+Command+Z → 播放暂停"));
    kp_binding_label(&c,display_binding,KP_SYSTEM_MACOS,label,1);assert(!*label);
    // A live configuration refresh cancels old HID input, retains local screen
    // and navigation state, and repairs the cursor if its profile was deleted.
    gesture_config(&c);kp_init(&m,&c,effect,NULL);memset(count,0,sizeof(count));
    m.page=KP_CONSOLE;m.executing=true;m.feedback.visible=true;
    m.down_mask=5;m.key[0]=(kp_key_t){.down=true,.pressed_at=100};
    m.key[2]=(kp_key_t){.down=true,.pressed_at=100,.pending=true,.clicks=1};
    m.screen.state=KP_SCREEN_OFF;m.screen.level=0;m.screen.last_activity=100;
    kp_screen_t before_screen=m.screen;unsigned before_revision=m.config_revision;
    kp_config_loaded(&m,200);
    assert(count[KP_CANCEL]==1&&!m.executing&&!m.feedback.visible);
    assert(m.page==KP_CONSOLE&&m.key[0].ignored&&m.key[0].down);
    assert(m.key[2].down&&m.key[2].pressed_at==100&&!m.key[2].pending&&!m.key[2].clicks);
    assert(!memcmp(&m.screen,&before_screen,sizeof(before_screen))&&m.config_revision==before_revision+1);
    m.page=KP_PROFILES;m.selected=7;m.down_mask=1;
    m.key[0]=(kp_key_t){.down=true,.pressed_at=300};kp_key_t navigation=m.key[0];
    kp_config_loaded(&m,400);
    assert(m.page==KP_PROFILES&&m.selected==c.active_profile);
    assert(!memcmp(&m.key[0],&navigation,sizeof(navigation))&&!memcmp(&m.screen,&before_screen,sizeof(before_screen)));
    puts("Live configuration preserves page, sleep and navigation while releasing old input and deleted-profile cursors: PASS");
    puts("System preview/save/back, filtered device labels and fixed HID modifiers: PASS");
    puts("Keypad gestures, navigation, screen drafts, wake consumption and feedback: PASS");
}
