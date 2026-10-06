#include "lvgl.h"
#include "src/misc/lv_text_private.h"
#include "keypad_ui.h"
#include "keypad_home_ui.h"
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
LV_FONT_DECLARE(passport_font_20_menu);
static void battery_layout(void){
    lv_obj_t *screen=lv_screen_active();bool found=false;
    for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
        lv_obj_t *o=lv_obj_get_child(screen,i);
        if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)||!lv_obj_check_type(o,&lv_label_class)||lv_obj_get_y(o)>=50)continue;
        if(strcmp(lv_label_get_text(o),"100%"))continue;
        const lv_font_t *font=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        lv_point_t size;lv_text_get_size(&size,"100%",font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(size.x>lv_obj_get_width(o)||lv_obj_get_height(o)!=font->line_height)
            fprintf(stderr,"Full battery: text width=%d label=%dx%d line=%d x=%d y=%d\n",(int)size.x,(int)lv_obj_get_width(o),(int)lv_obj_get_height(o),font->line_height,(int)lv_obj_get_x(o),(int)lv_obj_get_y(o));
        assert(size.x<=lv_obj_get_width(o)&&lv_obj_get_height(o)==font->line_height);
        assert(lv_obj_get_x(o)>=180-2&&lv_obj_get_x(o)+lv_obj_get_width(o)<=224);
        if(lv_obj_get_y(o)==23)assert(lv_obj_get_x(o)>=179); // Clear of the home battery icon.
        found=true;
    }
    assert(found);
}
static void menu_layout(unsigned expected){
    lv_obj_t *screen=lv_screen_active();unsigned rows=0;
    for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
        lv_obj_t *o=lv_obj_get_child(screen,i);
        if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_x(o)==20&&
           lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_20_menu){
            assert(lv_obj_get_x(o)==20&&lv_obj_get_y(o)==86+(int)rows*42);
            assert((lv_obj_get_width(o)==200||lv_obj_get_width(o)==113)&&lv_obj_get_height(o)==32);
            assert(lv_obj_get_style_text_line_space(o,LV_PART_MAIN)==0);
            assert(lv_label_get_long_mode(o)==LV_LABEL_LONG_DOT);
            ++rows;
        }
    }
    assert(rows==expected&&passport_font_20_menu.line_height<=32);
}
static void screen_layout(const kp_model_t *m){
    lv_obj_t *screen=lv_screen_active();unsigned values=0,rows=0;
    for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
        lv_obj_t *o=lv_obj_get_child(screen,i);
        if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)||!lv_obj_check_type(o,&lv_label_class))continue;
        const char *text=lv_label_get_text(o);const lv_font_t *font=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        for(uint32_t offset=0;text[offset];){
            uint32_t cp=lv_text_encoded_next(text,&offset);if(cp=='\n')continue;
            lv_font_glyph_dsc_t glyph;assert(lv_font_get_glyph_dsc(font,&glyph,cp,0)&&!glyph.is_placeholder);
        }
        assert(!strstr(text,"..."));
        assert(lv_obj_get_x(o)>=18&&lv_obj_get_x(o)+lv_obj_get_width(o)<=224);
        assert(lv_obj_get_y(o)>=18&&lv_obj_get_y(o)+lv_obj_get_height(o)<=311);
        if(m->page==KP_SCREEN&&lv_obj_get_x(o)==156){
            assert(lv_obj_get_y(o)==86+(int)values*42&&lv_obj_get_width(o)==64);
            assert(font==&passport_font_20_menu);values++;
        }
        if(m->page==KP_SCREEN&&lv_obj_get_x(o)==20){
            assert(lv_obj_get_y(o)==86+(int)rows*42&&lv_obj_get_width(o)==132);
            assert(font==&passport_font_20_menu);rows++;
        }
    }
    if(m->page==KP_SCREEN)assert(rows==4&&values==4);
}
static void console_layout(const char *state){
    lv_obj_t *screen=lv_screen_active();bool found=false,footer=false;unsigned cards=0;
    for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
        lv_obj_t *o=lv_obj_get_child(screen,i);if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))continue;
        if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),state)){
            assert(lv_obj_get_x(o)==18&&lv_obj_get_y(o)==20&&lv_obj_get_width(o)==151);
            assert(lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);
            assert(lv_color_eq(lv_obj_get_style_text_color(o,LV_PART_MAIN),lv_color_hex(0xd8a466)));found=true;
        }
        if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),"长按 OK 返回")){
            assert(lv_obj_get_y(o)==296&&lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);footer=true;
        }
        if(lv_obj_get_width(o)==212&&lv_obj_get_height(o)==80){
            assert(lv_obj_get_x(o)==14&&lv_obj_get_y(o)==46+(int)cards*84);unsigned marks=0;
            const char *gestures[]={"单","双","三","长"};
            for(unsigned n=0;n<lv_obj_get_child_count(o);n++){
                lv_obj_t *l=lv_obj_get_child(o,n);
                if(lv_obj_get_x(l)==48){
                    int y=lv_obj_get_y(l);
                    assert(y>=0&&y<=60&&y%20==0);
                    char prefix[32];snprintf(prefix,sizeof(prefix),"#%s %s#  ",y==40?"d8a466":"a3b5a5",gestures[y/20]);
                    assert(!strncmp(lv_label_get_text(l),prefix,strlen(prefix)));
                    assert(lv_label_get_recolor(l));
                    assert(lv_obj_get_style_text_font(l,LV_PART_MAIN)==&passport_font_16);
                    ++marks;
                }
            }
            assert(marks==4);++cards;
        }
    }
    assert(found&&footer&&cards==3);
}
static void save(lv_display_t *display,const char *directory,const char *name){
    if(getenv("KEYPAD_LAYOUT_DEBUG")){
        lv_mem_monitor_t memory;lv_mem_monitor(&memory);
        printf("Before %s: free=%zu largest=%zu peak=%zu\n",name,memory.free_size,memory.free_biggest_size,memory.max_used);fflush(stdout);
    }
    lv_refr_now(display);
    lv_obj_t *screen=lv_screen_active();
    assert(lv_obj_get_style_radius(screen,LV_PART_MAIN)==32&&!lv_obj_get_style_clip_corner(screen,LV_PART_MAIN));
    for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
        lv_obj_t *o=lv_obj_get_child(screen,i);
        if(lv_obj_get_y(o)==86&&lv_obj_get_width(o)==240&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_t *title=lv_obj_get_child(o,0);int h=lv_obj_get_height(title);
            assert(lv_obj_get_style_text_font(title,LV_PART_MAIN)==&passport_font_28);
            assert(h<=passport_font_28.line_height*2&&lv_obj_get_y(title)==(94-h)/2);
        }
    }
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
    coverage("assets/fonts/passport-codepoints.txt",&passport_font_20_menu,NULL);
    lv_font_glyph_dsc_t glyph;assert(!lv_font_get_glyph_dsc(&passport_font_16,&glyph,0x1f600,0)||glyph.is_placeholder);
    const lv_font_t *dynamic_fonts[]={&passport_font_16,&passport_font_20_menu,&passport_font_28};
    for(unsigned i=0;i<3;i++)assert(!lv_font_get_glyph_dsc(dynamic_fonts[i],&glyph,0x9f98,0)||glyph.is_placeholder);
    assert(!kp_codepoint_supported(0x3031));assert(passport_font_16.line_height<=24&&passport_font_28.line_height<=42);
    lv_display_t *d=lv_display_create(240,320);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
    kp_model_t m;kp_config_t c;kp_defaults(&c);kp_init(&m,&c,NULL,NULL);
    kp_home_config_t home;kp_home_defaults(&home);strcpy(home.name,"STARWING");strcpy(home.badge,"ID / 001");
    uint64_t epoch=1791167040;
    kp_ble_status_t ble={.connected=true,.ready=true,.secure=true,.keyboard_subscribed=true,.handles_ready=true};kp_ui_init();
    const char *names[]={"console","menu","settings","hotspot","forget","profiles","ap_confirm"};
    const unsigned row_counts[]={0,0,4,0,2,1,2};
    for(unsigned p=0;p<7;p++){m.page=p;kp_ble_status_t state=ble;if(p==KP_HOTSPOT){state.paused=true;state.ready=state.connected=state.secure=state.keyboard_subscribed=false;}kp_ui_update(&m,state,85,"","Passport-Keys-ABCD","12345678",&home,epoch);save(d,argv[1],names[p]);menu_layout(row_counts[p]);}
    // Render every real transfer state using the production UI, linked fonts
    // and the same 28 KiB LVGL pool. State updates must invalidate the cache.
    for (unsigned state = KP_TRANSFER_OFF; state <= KP_TRANSFER_STOPPING; ++state) {
        m.page = KP_TRANSFER; m.selected = state == KP_TRANSFER_OFF ? 0 : 1;
        m.transfer = (kp_transfer_view_t){.state = state, .enabled = state >= KP_TRANSFER_CONNECTING, .peers = 4};
        if (state == KP_TRANSFER_READY) strcpy(m.transfer.ip, "255.255.255.255");
        kp_ui_update(&m,ble,100,"","","",&home,epoch);lv_refr_now(d);screen_layout(&m);battery_layout();
        char name[40];snprintf(name,sizeof(name),"transfer_%u",state);save(d,argv[1],name);
        bool qr = false;
        for (unsigned i = 0; i < lv_obj_get_child_count(lv_screen_active()); ++i) {
            lv_obj_t *o = lv_obj_get_child(lv_screen_active(),i);
            if(lv_obj_check_type(o,&lv_qrcode_class)) {qr=true;assert(state==KP_TRANSFER_READY&&lv_obj_get_width(o)==99&&lv_obj_get_x(o)==20&&lv_obj_get_y(o)==164);}
            if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)||!lv_obj_check_type(o,&lv_label_class))continue;
            const char *text=lv_label_get_text(o);const lv_font_t *font=lv_obj_get_style_text_font(o,LV_PART_MAIN);
            lv_point_t size;lv_text_get_size(&size,text,font,lv_obj_get_style_text_letter_space(o,0),lv_obj_get_style_text_line_space(o,0),lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
            if(size.y>lv_obj_get_height(o))fprintf(stderr,"Clipped transfer text %s: %d > %d\n",text,(int)size.y,(int)lv_obj_get_height(o));
            assert(size.y<=lv_obj_get_height(o));
        }
        assert(qr==(state==KP_TRANSFER_READY));
    }
    m.transfer=(kp_transfer_view_t){0};m.page=KP_SETTINGS;m.selected=4;
    kp_ui_update(&m,ble,100,"","","",&home,epoch);lv_refr_now(d);menu_layout(4);screen_layout(&m);save(d,argv[1],"settings_scrolled");
    m.selected=0;
    m.screen_draft=m.screen_config;
    for(unsigned p=0;p<=KP_SYSTEM;p++){
        m.page=p;kp_ui_update(&m,ble,100,"","Passport-Keys-ABCD","12345678",&home,epoch);lv_refr_now(d);battery_layout();
        if(p==KP_MENU||p==KP_SETTINGS)save(d,argv[1],p==KP_MENU?"home_full_battery":"settings_full_battery");
    }
    // Render the production system page, including saved/preview/failure states.
    const char *system_images[]={"system_mac","system_pending_windows","system_windows","system_save_failed"};
    for(unsigned state=0;state<4;state++){
        m.page=KP_SYSTEM;m.system=state>=2?KP_SYSTEM_WINDOWS:KP_SYSTEM_MACOS;m.selected=state==0||state==3?0:1;
        m.system_saved=state==2;m.system_save_failed=state==3;++m.config_revision;
        kp_ui_update(&m,ble,100,"","","",&home,epoch);save(d,argv[1],system_images[state]);screen_layout(&m);menu_layout(2);battery_layout();
        // Every single line must fit at its actual selected font, without wrapping.
        lv_obj_t *screen=lv_screen_active();
        for(unsigned i=0;i<lv_obj_get_child_count(screen);i++){
            lv_obj_t *o=lv_obj_get_child(screen,i);
            if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)||!lv_obj_check_type(o,&lv_label_class))continue;
            const char *text=lv_label_get_text(o);lv_point_t size;
            lv_text_get_size(&size,text,lv_obj_get_style_text_font(o,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            assert(!strchr(text,'\n')&&size.x<=lv_obj_get_width(o));
        }
    }
    m.system=KP_SYSTEM_WINDOWS;m.page=KP_SETTINGS;m.selected=1;++m.config_revision;
    kp_ui_update(&m,ble,100,"","","",&home,epoch);save(d,argv[1],"settings_windows");menu_layout(4);screen_layout(&m);
    // Repeated recreation of mutually exclusive label roles stays in the 28 KiB pool.
    for(unsigned i=0;i<100;i++){
        m.page=i%3==0?KP_SYSTEM:i%3==1?KP_SETTINGS:KP_CONSOLE;++m.config_revision;
        kp_ui_update(&m,ble,100,"","","",&home,epoch);lv_refr_now(d);
    }
    m.page=KP_SCREEN;m.selected=2;kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"screen_settings");screen_layout(&m);
    const char *edit_names[]={"screen_normal","screen_dim_time","screen_dim_level","screen_off_time"};
    for(unsigned setting=0;setting<KP_SCREEN_SETTING_COUNT;setting++){
        m.page=KP_SCREEN_EDIT;m.screen_setting=setting;m.screen_draft=m.screen_config;++m.config_revision;
        kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],edit_names[setting]);screen_layout(&m);
    }
    m.screen_draft.off_after_s=0;++m.config_revision;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"screen_disabled");screen_layout(&m);
    m.screen_draft.off_after_s=300;++m.config_revision;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"screen_300s");screen_layout(&m);
    m.screen_save_failed=true;++m.config_revision;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"screen_save_failed");screen_layout(&m);m.screen_save_failed=false;
    for(unsigned i=0;i<100;i++){
        m.page=i%2?KP_SCREEN:KP_SCREEN_EDIT;m.screen_setting=KP_SCREEN_DIM_BRIGHTNESS;
        m.screen_draft=m.screen_config;++m.config_revision;
        kp_ui_update(&m,ble,85,"","","",&home,epoch);lv_refr_now(d);
    }
    m.page=KP_MENU;
    for(unsigned avatar=0;avatar<4;avatar++){
        home.avatar=avatar;kp_ui_update(&m,ble,82,"","","",&home,epoch);
        char name[32];snprintf(name,sizeof(name),"home-avatar-%u",avatar);save(d,argv[1],name);
    }
    home.avatar=0;
    for(unsigned day=0;day<7;day++){
        kp_ui_update(&m,ble,82,"","","",&home,epoch+day*86400);
        char name[32];snprintf(name,sizeof(name),"home-weekday-%u",day);save(d,argv[1],name);
    }
    for(unsigned selected=0;selected<3;selected++){
        m.selected=selected;kp_ui_update(&m,ble,82,"","","",&home,epoch);
        char name[32];snprintf(name,sizeof(name),"home-selected-%u",selected);save(d,argv[1],name);
        unsigned menu_count=0;
        for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++){
            lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
            if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_x(o)==128){
                assert(lv_obj_get_width(o)==76&&lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);
                bool active=lv_obj_get_y(o)==233+(int)selected*24;
                assert(lv_color_eq(lv_obj_get_style_text_color(o,LV_PART_MAIN),lv_color_hex(active?0x18241f:0xe9ecdf)));menu_count++;
            }
            if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){
                if(lv_obj_get_x(o)==30&&lv_obj_get_y(o)==233)assert(lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);
                if(lv_obj_get_x(o)==20&&lv_obj_get_y(o)==257){assert(!strcmp(lv_label_get_text(o),"SCENE"));assert(lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);}
                if(lv_obj_get_x(o)==20&&lv_obj_get_y(o)==277)assert(lv_obj_get_style_text_font(o,LV_PART_MAIN)==&passport_font_16);
            }
        }assert(menu_count==3);
    }
    m.selected=0;
    for(unsigned n=0;n<7;n++){
        kp_ble_status_t b={0};if(n==0)b=ble;
        if(n==2)b.connected=b.handles_ready=true;
        if(n==3)b.connected=true;if(n==4)b.connected=b.handles_ready=b.secure=true;
        if(n==5)b.failed=true;if(n==6)b.paused=true;
        kp_ui_update(&m,b,n==5?-1:82,"","","",&home,epoch);
        char name[32];snprintf(name,sizeof(name),"home-link-%u",n);save(d,argv[1],name);
    }
    strcpy(home.name,"星翼");kp_ui_update(&m,ble,82,"","","",&home,epoch);save(d,argv[1],"home-chinese");
    strcpy(home.name,"浏览情景长名称测试全选后复制窗口");
    assert(kp_profile_name_set(&m.config,0,"浏览情景长名称测试全选后复制窗口"));++m.config_revision;
    kp_ui_update(&m,ble,82,"","","",&home,epoch);save(d,argv[1],"home-long");
    unsigned dotted=0;
    for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++){
        lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
        if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&
            (lv_obj_get_y(o)==79||lv_obj_get_y(o)==277)){assert(strstr(lv_label_get_text(o),"..."));++dotted;}
    }assert(dotted==2);
    home.hour12=1;kp_ui_update(&m,ble,82,"","","",&home,epoch);save(d,argv[1],"home-hour12");
    kp_ui_update(&m,ble,-1,"","","",&home,0);save(d,argv[1],"home-unset");
    strcpy(home.name,"STARWING");home.hour12=0;assert(kp_profile_name_set(&m.config,0,"默认"));++m.config_revision;
    for(unsigned n=0;n<100;n++){
        m.page=n%2?KP_CONSOLE:KP_MENU;
        kp_ui_update(&m,ble,82,"","","",&home,epoch);lv_refr_now(d);
    }
    m.page=KP_CONSOLE;kp_ui_update(&m,ble,85,"","","",&home,epoch);lv_refr_now(d);console_layout("默认 · 已连接");
    const char *states[]={"等待电脑连接","等待配对完成","键盘服务未就绪","等待电脑启用键盘","蓝牙异常","蓝牙已暂停"};
    const char *screens[]={"console_waiting","console_pairing","console_service","console_subscription","console_failed","console_paused"};
    for(unsigned n=0;n<6;n++){
        kp_ble_status_t b={0};if(n==1)b.connected=b.handles_ready=true;
        if(n==2)b.connected=true;if(n==3)b.connected=b.handles_ready=b.secure=true;
        if(n==4)b.failed=true;if(n==5)b.paused=true;
        kp_ui_update(&m,b,85,"","","",&home,epoch);save(d,argv[1],screens[n]);console_layout(states[n]);
    }
    kp_ui_update(&m,ble,85,"","Passport-Keys-ABCD","12345678",&home,epoch);
    unsigned locks=lock_count;m.key[0].pressed_at++;m.down_mask=1;kp_ui_update(&m,ble,85,"","Passport-Keys-ABCD","12345678",&home,epoch);assert(lock_count==locks);
    m.selected=1;kp_ui_update(&m,ble,85,"","Passport-Keys-ABCD","12345678",&home,epoch);assert(lock_count==locks+1);
    m.page=KP_CONSOLE;m.selected=0;m.feedback=(kp_feedback_t){.visible=true,.slot=8,.completed=1,.total=1,.state=KP_FEEDBACK_DONE,.serial=1};
    kp_ui_update(&m,ble,85,"已完成","","",&home,epoch);save(d,argv[1],"command_enter");
    m.feedback=(kp_feedback_t){.visible=true,.slot=10,.completed=1,.total=1,.state=KP_FEEDBACK_DONE,.serial=10};
    kp_ui_update(&m,ble,85,"已完成","","",&home,epoch);save(d,argv[1],"command_triple");
    bool triple=false;for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++){
        lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
        if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(o),"OK · 三击"))triple=true;
    }assert(triple);
    kp_step_t steps[2]={{{.type=KP_KEYBOARD,.modifiers=8,.keys={4}},100},{{.type=KP_KEYBOARD,.modifiers=8,.keys={6}},0}};
    assert(kp_binding_set(&m.config,0,4,"全选后复制",steps,2));++m.config_revision;
    m.feedback=(kp_feedback_t){.visible=true,.slot=4,.completed=1,.total=2,.state=KP_FEEDBACK_RUNNING,.serial=2};
    kp_ui_update(&m,ble,85,"正在执行","","",&home,epoch);save(d,argv[1],"command_macro");
    m.feedback=(kp_feedback_t){.visible=true,.slot=3,.total=1,.holding=true,.state=KP_FEEDBACK_RUNNING,.serial=3};
    kp_ui_update(&m,ble,85,"正在执行","","",&home,epoch);save(d,argv[1],"command_hold");
    m.feedback=(kp_feedback_t){.visible=true,.slot=4,.completed=1,.total=2,.state=KP_FEEDBACK_FAILED,.serial=4};kp_ui_update(&m,ble,85,"发送失败","","",&home,epoch);save(d,argv[1],"command_failed");
    assert(kp_binding_set(&m.config,0,4,"全选后复制并打开新的编辑窗口测试",steps,2));++m.config_revision;
    m.feedback.state=KP_FEEDBACK_DONE;m.feedback.completed=2;
    kp_ui_update(&m,ble,85,"已完成","","",&home,epoch);save(d,argv[1],"command_long");
    m.feedback.visible=false;m.page=KP_PROFILES;
    const char *profiles[]={"Coding","浏览","写作","演示","终端","视频","浏览情景长名称测试"};
    for(unsigned i=0;i<7;i++)assert(kp_profile_add(&m.config,profiles[i]));++m.config_revision;m.selected=7;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"profiles_long");
    menu_layout(4);
    m.page=KP_CONSOLE;m.config.active_profile=7;
    for(unsigned i=0;i<KP_BINDINGS;i++)assert(kp_binding_set(&m.config,7,i,"全选后复制并打开新的编辑窗口测试",steps,2));++m.config_revision;
    kp_ui_update(&m,ble,85,"保存失败，仍使用原情景","","",&home,epoch);save(d,argv[1],"console_long");
    assert(kp_profile_name_set(&m.config,7,"浏览情景长名称测试全选后复制窗口"));++m.config_revision;m.page=KP_PROFILES;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"profiles_max");menu_layout(4);
    bool ellipsized=false;
    for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++){
        lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
        if(lv_obj_check_type(o,&lv_label_class)&&!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_y(o)==212)
            ellipsized=strstr(lv_label_get_text(o),"...")!=NULL;
    }
    assert(ellipsized);
    assert(!kp_profile_name_set(&m.config,7,"龘靐齉罍鱻麤灥"));
    // A name saved by the full-inventory firmware remains usable, with visible
    // missing-glyph placeholders instead of discarding its shortcut mappings.
    memcpy(m.config.data+m.config.profiles[7].name,"龘",3);
    assert(kp_config_valid(&m.config));++m.config_revision;m.page=KP_PROFILES;
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"profiles_rare");menu_layout(4);
    kp_step_t maximum[KP_MAX_STEPS];
    for(unsigned i=0;i<KP_MAX_STEPS;i++)maximum[i]=(kp_step_t){{.type=KP_KEYBOARD,.modifiers=255,.keys={4,5,6,7,8,9}},i+1==KP_MAX_STEPS?0:80};
    assert(kp_binding_set(&m.config,0,0,"全选后复制并打开新的编辑窗口测试",maximum,KP_MAX_STEPS));
    m.page=KP_CONSOLE;m.config.active_profile=0;++m.config_revision;m.feedback=(kp_feedback_t){.visible=true,.slot=0,.total=64,.state=KP_FEEDBACK_DONE};
    kp_ui_update(&m,ble,85,"","","",&home,epoch);save(d,argv[1],"command_capacity");
    // Match the firmware's 28 KiB allocator and repeatedly reuse the detail objects.
    for(unsigned i=0;i<100;i++){
        m.feedback.visible=i%2==0;++m.feedback.serial;
        kp_ui_update(&m,ble,85,"","","",&home,epoch);lv_refr_now(d);
    }
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);assert(lv_mem_test()==LV_RESULT_OK&&memory.free_biggest_size>512);
    printf("LVGL 28 KiB pool: peak=%zu free=%zu largest=%zu\n",memory.max_used,memory.free_size,memory.free_biggest_size);
    puts("LVGL live glyph lookup, pages, feedback, long text, repeated commands and cache: PASS");return 0;
}
