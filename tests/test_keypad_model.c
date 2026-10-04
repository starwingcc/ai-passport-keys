#include "keypad_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count[KP_CLEAR_BOND + 1];
static kp_action_t last;
static void effect(void *ctx, kp_effect_t e, const kp_action_t *a) {
    (void)ctx; ++count[e]; if (a) last = *a;
}
static void press(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m, k, true, t); }
static void release(kp_model_t *m, unsigned k, uint32_t t) { kp_input(m, k, false, t); }
static void click(kp_model_t *m, unsigned k, uint32_t t) { press(m,k,t); release(m,k,t+40); kp_tick(m,t+220); }
static void check_default_gestures(void) {
    kp_model_t m; kp_init(&m,NULL,effect,NULL);
    uint8_t keyboard[8], consumer[2];
    const uint8_t zero[8] = {0};
    click(&m,0,0);
    assert(count[KP_TAP]==1 && last.type==KP_KEYBOARD);
    kp_report(&last,keyboard,consumer);
    const uint8_t esc[8] = {0,0,0x29};
    assert(!memcmp(keyboard,esc,8) && consumer[0]==0 && consumer[1]==0);

    press(&m,0,300); kp_tick(&m,799); assert(count[KP_HOLD]==0);
    kp_tick(&m,800); assert(count[KP_HOLD]==1);
    kp_report(&last,keyboard,consumer);
    const uint8_t option[8] = {0x04};
    assert(!memcmp(keyboard,option,8));
    kp_tick(&m,1200); assert(count[KP_HOLD]==1 && count[KP_RELEASE]==0);
    release(&m,0,1300); assert(count[KP_RELEASE]==1);
    kp_report(NULL,keyboard,consumer); assert(!memcmp(keyboard,zero,8));
    kp_tick(&m,1600); assert(count[KP_TAP]==1);

    click(&m,1,1700); assert(count[KP_TAP]==2);
    kp_report(&last,keyboard,consumer);
    const uint8_t backspace[8] = {0,0,0x2a};
    assert(!memcmp(keyboard,backspace,8));
    press(&m,1,2000); release(&m,1,2040);
    press(&m,1,2100); release(&m,1,2140); assert(count[KP_TAP]==3);
    kp_report(&last,keyboard,consumer);
    const uint8_t select_all[8] = {0x08,0,0x04};
    assert(!memcmp(keyboard,select_all,8));
    kp_tick(&m,2400); assert(count[KP_TAP]==3);

    click(&m,2,2500); assert(count[KP_TAP]==4);
    kp_report(&last,keyboard,consumer);
    const uint8_t enter[8] = {0,0,0x28};
    assert(!memcmp(keyboard,enter,8));
    assert(m.config.bindings[1].type==KP_NONE && m.config.bindings[7].type==KP_NONE);
    assert(m.config.bindings[5].keys[0]==0x51);
    memset(count,0,sizeof(count));
}
int main(void) {
    check_default_gestures();
    kp_config_t c; kp_defaults(&c); assert(kp_config_valid(&c));
    kp_model_t m; kp_init(&m,&c,effect,NULL);
    press(&m,0,0); release(&m,0,50); kp_tick(&m,229); assert(count[KP_TAP]==0);
    kp_tick(&m,230); assert(count[KP_TAP]==1 && last.keys[0]==0x29);
    c.bindings[1]=(kp_action_t){.type=KP_KEYBOARD,.modifiers=8,.keys={6}};
    m.config=c;
    press(&m,0,300); release(&m,0,340); press(&m,0,400); release(&m,0,440);
    assert(count[KP_TAP]==2 && last.modifiers==8 && last.keys[0]==6);
    kp_tick(&m,800); assert(count[KP_TAP]==2);
    press(&m,1,900); kp_tick(&m,1399); assert(count[KP_HOLD]==0);
    kp_tick(&m,1400); assert(count[KP_HOLD]==1); release(&m,1,1500);
    assert(count[KP_RELEASE]==1); kp_tick(&m,2000); assert(count[KP_TAP]==2);
    press(&m,2,2100); kp_tick(&m,2600); assert(m.page==KP_MENU && m.blocked);
    release(&m,2,2800); assert(!m.blocked && m.page==KP_MENU);
    kp_tick(&m,3100); assert(count[KP_TAP]==2);
    press(&m,2,3200); kp_tick(&m,3700); release(&m,2,3800); assert(m.page==KP_MENU);
    click(&m,1,3900); click(&m,2,4200); assert(m.page==KP_SETTINGS);
    click(&m,2,4500); assert(m.page==KP_HOTSPOT && count[KP_AP_START]==1);
    click(&m,2,4800); assert(m.page==KP_HOTSPOT);
    press(&m,2,5100); kp_tick(&m,5600); release(&m,2,5700);
    assert(m.page==KP_SETTINGS && count[KP_AP_STOP]==1);
    click(&m,1,5800); click(&m,2,6100); assert(m.page==KP_FORGET && m.selected==0);
    click(&m,2,6400); assert(m.page==KP_SETTINGS && count[KP_CLEAR_BOND]==0);
    click(&m,1,6700); click(&m,2,7000); click(&m,1,7300); click(&m,2,7600);
    assert(count[KP_CLEAR_BOND]==1 && m.page==KP_SETTINGS);
    // Cancel queued single clicks and suppress held input on link/page changes.
    kp_init(&m,&c,effect,NULL); unsigned taps=count[KP_TAP];
    press(&m,0,100); release(&m,0,120); kp_cancel(&m); kp_tick(&m,600); assert(count[KP_TAP]==taps);
    press(&m,0,700); kp_cancel(&m); kp_tick(&m,1400); assert(m.blocked);
    release(&m,0,1500); click(&m,0,1600); assert(count[KP_TAP]==taps+1);
    // A second press held through the threshold becomes a hold, not a double.
    kp_init(&m,&c,effect,NULL); taps=count[KP_TAP];
    press(&m,0,0); release(&m,0,30); press(&m,0,100); kp_tick(&m,600); release(&m,0,700);
    kp_tick(&m,1000); assert(count[KP_TAP]==taps);
    // Millisecond counter wrap.
    kp_init(&m,&c,effect,NULL); unsigned holds=count[KP_HOLD];
    press(&m,0,UINT32_MAX-200); kp_tick(&m,299); assert(count[KP_HOLD]==holds+1);
    release(&m,0,350);
    uint8_t k[8],media[2]; kp_report(&c.bindings[1],k,media); assert(k[0]==8 && k[2]==6 && media[0]==0);
    kp_action_t a={.type=KP_MEDIA,.consumer=0xe9}; kp_report(&a,k,media); assert(k[0]==0 && media[0]==0xe9);
    kp_report(NULL,k,media); for(unsigned i=0;i<8;i++)assert(k[i]==0); assert(media[0]==0);
    c.bindings[0].keys[0]=0x66; assert(!kp_config_valid(&c));
    kp_defaults(&c); c.bindings[0].keys[1]=c.bindings[0].keys[0]; assert(!kp_config_valid(&c));
    kp_defaults(&c); c.bindings[0].type=KP_NONE; assert(!kp_config_valid(&c));
    kp_defaults(&c); c.bindings[0]=(kp_action_t){.type=KP_KEYBOARD,.modifiers=0x80};assert(kp_config_valid(&c));
    char text[80]; kp_action_label(&c.bindings[0],text,sizeof(text)); assert(!strcmp(text,"RCmd"));
    a = (kp_action_t){.type=KP_MEDIA,.consumer=0xb5};
    kp_action_label(&a,text,sizeof(text)); assert(!strcmp(text,"下一首"));
    a.consumer=0xb6; kp_action_label(&a,text,sizeof(text)); assert(!strcmp(text,"上一首"));
    // Every configurable slot routes its own chord, including OK double click.
    for (unsigned slot=0;slot<KP_BINDINGS;slot++) {
        kp_defaults(&c);
        c.bindings[slot]=(kp_action_t){.type=KP_KEYBOARD,.keys={4+slot}};
        kp_init(&m,&c,effect,NULL); memset(count,0,sizeof(count));
        unsigned key=slot/3, gesture=slot%3;
        press(&m,key,0);
        if (gesture==2) {
            kp_tick(&m,499); assert(count[KP_HOLD]==0);
            kp_tick(&m,500); assert(count[KP_HOLD]==1 && last.keys[0]==4+slot);
            release(&m,key,600); assert(count[KP_RELEASE]==1);
        } else {
            release(&m,key,40);
            if (gesture==1) { press(&m,key,100); release(&m,key,140); }
            kp_tick(&m,400); assert(count[KP_TAP]==1 && last.keys[0]==4+slot);
        }
    }
    kp_init(&m,&c,effect,NULL); memset(count,0,sizeof(count));
    press(&m,2,0); kp_tick(&m,499); assert(m.page==KP_CONSOLE);
    kp_tick(&m,500); assert(m.page==KP_MENU && count[KP_TAP]==0 && count[KP_HOLD]==0);
    release(&m,2,600); kp_tick(&m,1000); assert(m.page==KP_MENU && count[KP_TAP]==0);
    puts("Keypad model: PASS");
}
