#include "lvgl.h"
#include "keypad_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t buffer[240*320*2], pixels[240*320*2];
static unsigned lock_count;
bool bsp_lvgl_lock(int ms) { (void)ms; ++lock_count; return true; }
void bsp_lvgl_unlock(void) {}
static void flush(lv_display_t *display,const lv_area_t *a,uint8_t *data) {
    for(int y=a->y1;y<=a->y2;y++) {
        memcpy(pixels+(y*240+a->x1)*2,data,(a->x2-a->x1+1)*2);
        data+=(a->x2-a->x1+1)*2;
    }
    lv_display_flush_ready(display);
}
LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
int main(int argc,char **argv) {
    assert(argc==2);
    lv_init();
    // Exercise LVGL's actual descriptor lookup, including a missing-glyph case.
    FILE *inventory=fopen("assets/fonts/passport-codepoints.txt","r"); assert(inventory);
    unsigned cp; lv_font_glyph_dsc_t glyph;
    while(fscanf(inventory,"U+%x\n",&cp)==1) {
        assert(lv_font_get_glyph_dsc(&passport_font_16,&glyph,cp,0)&&!glyph.is_placeholder);
        assert(lv_font_get_glyph_dsc(&passport_font_20,&glyph,cp,0)&&!glyph.is_placeholder);
    }
    fclose(inventory);
    assert(!lv_font_get_glyph_dsc(&passport_font_16,&glyph,0x9f98,0)||glyph.is_placeholder);
    lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d,flush);
    kp_model_t model;kp_config_t config;kp_defaults(&config);kp_init(&model,&config,NULL,NULL);
    kp_ble_status_t ble={.connected=true,.ready=true,.secure=true,
                         .keyboard_subscribed=true,.handles_ready=true};
    kp_ui_init();
    const char *names[]={"console","menu","settings","hotspot","forget"};
    for(unsigned p=0;p<sizeof(names)/sizeof(names[0]);p++) {
        model.page=p;
        kp_ble_status_t display_ble = ble;
        if (model.page == KP_HOTSPOT) {
            display_ble.paused = true;
            display_ble.connected = display_ble.ready = display_ble.secure = false;
            display_ble.keyboard_subscribed = false;
        }
        kp_ui_update(&model,display_ble,85,"", "Passport-Keys-ABCD","AbcDef234567");
        lv_refr_now(d);
        char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",argv[1],names[p]);
        FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 320\n255\n");
        for(unsigned i=0;i<240*320;i++) {
            uint16_t v=pixels[i*2]|(pixels[i*2+1]<<8);
            uint8_t rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
            fwrite(rgb,1,3,out);
        }
        fclose(out);
    }
    unsigned locks = lock_count;
    model.key[0].pressed_at++;
    model.down_mask = 1;
    kp_ui_update(&model,ble,85,"", "Passport-Keys-ABCD","AbcDef234567");
    assert(lock_count == locks); // Invisible gesture state must not redraw UI.
    model.selected = 1;
    kp_ui_update(&model,ble,85,"", "Passport-Keys-ABCD","AbcDef234567");
    assert(lock_count == locks + 1);
    puts("LVGL glyph lookup, five-page render and render cache: PASS");
    return 0;
}
