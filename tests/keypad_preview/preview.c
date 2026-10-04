#include "lvgl.h"
#include "keypad_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t buffer[240*320*2],pixels[240*320*2];static unsigned lock_count;
bool bsp_lvgl_lock(int ms){(void)ms;++lock_count;return true;}
void bsp_lvgl_unlock(void){}
static void flush(lv_display_t *display,const lv_area_t *a,uint8_t *data){
    for(int y=a->y1;y<=a->y2;y++){memcpy(pixels+(y*240+a->x1)*2,data,(a->x2-a->x1+1)*2);data+=(a->x2-a->x1+1)*2;}lv_display_flush_ready(display);
}
LV_FONT_DECLARE(passport_font_16);LV_FONT_DECLARE(passport_font_20);LV_FONT_DECLARE(passport_font_28);
static void save(lv_display_t *display,const char *directory,const char *name){
    lv_refr_now(display);
    if(getenv("KEYPAD_LAYOUT_DEBUG")){
        lv_obj_t *screen=lv_screen_active();
        for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
            lv_obj_t *o=lv_obj_get_child(screen,i);
            if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){
                lv_area_t a;lv_obj_get_coords(o,&a);printf("%s [%d,%d,%d,%d] %.120s\n",name,(int)a.x1,(int)a.y1,(int)a.x2,(int)a.y2,lv_label_get_text(o));
            }
        }
    }
    char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 320\n255\n");
    for(unsigned i=0;i<240*320;i++){uint16_t v=pixels[i*2]|(pixels[i*2+1]<<8);uint8_t rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,1,3,out);}fclose(out);
}
static void coverage(const char *file,const lv_font_t *a,const lv_font_t *b){FILE *f=fopen(file,"r");assert(f);unsigned cp;lv_font_glyph_dsc_t glyph;
    while(fscanf(f,"U+%x\n",&cp)==1){assert(lv_font_get_glyph_dsc(a,&glyph,cp,0)&&!glyph.is_placeholder);if(b)assert(lv_font_get_glyph_dsc(b,&glyph,cp,0)&&!glyph.is_placeholder);}fclose(f);
}
int main(int argc,char **argv){
    assert(argc==2);lv_init();coverage("assets/fonts/passport-codepoints.txt",&passport_font_16,&passport_font_28);coverage("assets/fonts/passport-ui-codepoints.txt",&passport_font_20,NULL);
    lv_font_glyph_dsc_t glyph;assert(!lv_font_get_glyph_dsc(&passport_font_16,&glyph,0x1f600,0)||glyph.is_placeholder);
    assert(!kp_codepoint_supported(0x3031));assert(passport_font_16.line_height<=24&&passport_font_28.line_height<=42);
    lv_display_t *d=lv_display_create(240,320);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
    kp_model_t m;kp_config_t c;kp_defaults(&c);kp_init(&m,&c,NULL,NULL);
    kp_ble_status_t ble={.connected=true,.ready=true,.secure=true,.keyboard_subscribed=true,.handles_ready=true};kp_ui_init();
    const char *names[]={"console","menu","settings","hotspot","forget","profiles"};
    for(unsigned p=0;p<6;p++){m.page=p;kp_ble_status_t state=ble;if(p==KP_HOTSPOT){state.paused=true;state.ready=state.connected=state.secure=state.keyboard_subscribed=false;}kp_ui_update(&m,state,85,"","Passport-Keys-ABCD","12345678");save(d,argv[1],names[p]);}
    unsigned locks=lock_count;m.key[0].pressed_at++;m.down_mask=1;kp_ui_update(&m,ble,85,"","Passport-Keys-ABCD","12345678");assert(lock_count==locks);
    m.selected=1;kp_ui_update(&m,ble,85,"","Passport-Keys-ABCD","12345678");assert(lock_count==locks+1);
    m.page=KP_CONSOLE;m.selected=0;m.feedback=(kp_feedback_t){.visible=true,.slot=6,.completed=1,.total=1,.state=KP_FEEDBACK_DONE,.serial=1};
    kp_ui_update(&m,ble,85,"已完成","","");save(d,argv[1],"command_enter");
    kp_step_t steps[2]={{{.type=KP_KEYBOARD,.modifiers=8,.keys={4}},100},{{.type=KP_KEYBOARD,.modifiers=8,.keys={6}},0}};
    assert(kp_binding_set(&m.config,0,3,"全选后复制","复制编辑器里的全部内容",steps,2));++m.config_revision;
    m.feedback=(kp_feedback_t){.visible=true,.slot=3,.completed=1,.total=2,.state=KP_FEEDBACK_RUNNING,.serial=2};
    kp_ui_update(&m,ble,85,"正在执行","","");save(d,argv[1],"command_macro");
    m.feedback.state=KP_FEEDBACK_FAILED;kp_ui_update(&m,ble,85,"发送失败","","");save(d,argv[1],"command_failed");
    assert(kp_binding_set(&m.config,0,3,"全选后复制并打开新的编辑窗口","这是一段用于检验中文换行和省略显示的较长描述。设备会显示清晰的标题、副标题和描述，超过屏幕的部分应当省略。",steps,2));++m.config_revision;
    kp_ui_update(&m,ble,85,"发送失败","","");save(d,argv[1],"command_long");
    m.feedback.visible=false;m.page=KP_PROFILES;
    const char *profiles[]={"Coding","浏览","写作","演示","终端","视频","浏览情景长名称测试"};
    for(unsigned i=0;i<7;i++)assert(kp_profile_add(&m.config,profiles[i]));++m.config_revision;m.selected=7;
    kp_ui_update(&m,ble,85,"","","");save(d,argv[1],"profiles_long");
    m.page=KP_CONSOLE;m.config.active_profile=7;
    for(unsigned i=0;i<KP_BINDINGS;i++)assert(kp_binding_set(&m.config,7,i,"全选后复制并打开新的编辑窗口","",steps,2));++m.config_revision;
    kp_ui_update(&m,ble,85,"保存失败，仍使用原情景","","");save(d,argv[1],"console_long");
    kp_step_t maximum[KP_MAX_STEPS];
    for(unsigned i=0;i<KP_MAX_STEPS;i++)maximum[i]=(kp_step_t){{.type=KP_KEYBOARD,.modifiers=255,.keys={4,5,6,7,8,9}},i+1==KP_MAX_STEPS?0:80};
    char description[193];for(unsigned i=0;i<64;i++)memcpy(description+i*3,"字",3);description[192]=0;
    assert(kp_binding_set(&m.config,0,0,"全选后复制并打开新的编辑窗口",description,maximum,KP_MAX_STEPS));
    m.config.active_profile=0;++m.config_revision;m.feedback=(kp_feedback_t){.visible=true,.slot=0,.total=64,.state=KP_FEEDBACK_DONE};
    kp_ui_update(&m,ble,85,"","","");save(d,argv[1],"command_capacity");
    // Match the firmware's 24 KiB allocator and repeatedly reuse the detail objects.
    for(unsigned i=0;i<100;i++){
        m.feedback.visible=i%2==0;++m.feedback.serial;
        kp_ui_update(&m,ble,85,"","","");lv_refr_now(d);
    }
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);assert(lv_mem_test()==LV_RESULT_OK&&memory.free_biggest_size>512);
    printf("LVGL 24 KiB pool: peak=%zu free=%zu largest=%zu\n",memory.max_used,memory.free_size,memory.free_biggest_size);
    puts("LVGL live glyph lookup, pages, feedback, long text, repeated commands and cache: PASS");return 0;
}
