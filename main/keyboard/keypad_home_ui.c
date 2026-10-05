#include "keypad_home_ui.h"
#include <assert.h>
#include <string.h>

/* Geometry and sprites from the accepted 240x320 card preview. */
enum { CONTENT_OFFSET_Y=-8, FOOTER_Y=232, BG=0x18241f, INK=0xe9ecdf, GOLD=0xd8a466,
       MUTED=0xa3b5a5, PANEL=0x25362d, EDGE=0x617467 };
static bool visible;
static unsigned selected;
static int sample_battery;
static uint32_t status_color;
static char clock_text[6], date_text[15], period_text[3];
static uint8_t sprite[24][24];
static const uint32_t palette[]={PANEL, MUTED, INK, GOLD, BG, EDGE};

static void box(lv_layer_t *layer,int x,int y,int w,int h,uint32_t color) {
    if (w <= 0 || h <= 0) return;
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(color);d.bg_opa=LV_OPA_COVER;
    d.border_width=0;d.radius=0;
    lv_area_t area={x,y,x+w-1,y+h-1};lv_draw_rect(layer,&d,&area);
}
static void spr(int x,int y,int w,int h,uint8_t c) {
    assert(x>=0&&y>=0&&x+w<=24&&y+h<=24);
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)sprite[yy][xx]=c;
}
static void make_sprite(int kind) {
    memset(sprite,0,sizeof(sprite));
    if(kind==0) {
        spr(11,1,2,5,1);spr(10,0,4,2,2);
        spr(6,6,12,2,1);spr(4,8,16,9,1);spr(6,17,12,2,1);
        spr(2,10,2,5,1);spr(20,10,2,5,1);spr(6,9,12,7,4);
        spr(8,11,2,3,3);spr(14,11,2,3,3);
        spr(10,19,4,1,5);spr(7,20,10,3,1);spr(4,21,3,3,1);spr(17,21,3,3,1);
        spr(10,21,4,2,4);
    } else if(kind==1) {
        spr(4,3,4,7,1);spr(16,3,4,7,1);spr(5,5,2,3,3);spr(17,5,2,3,3);
        spr(4,9,16,8,1);spr(6,17,12,2,1);spr(8,11,8,7,2);
        spr(6,11,2,3,4);spr(16,11,2,3,4);spr(11,14,2,1,4);
        spr(9,16,2,1,4);spr(13,16,2,1,4);spr(1,13,4,1,2);spr(19,13,4,1,2);
        spr(7,20,10,4,2);spr(9,20,6,1,4);spr(11,20,2,2,3);
    } else if(kind==2) {
        spr(8,2,8,2,2);spr(5,4,14,2,2);spr(3,6,18,11,2);spr(5,17,14,2,2);
        spr(6,6,12,2,4);spr(5,8,14,7,4);spr(7,15,10,2,4);
        spr(1,8,2,7,3);spr(21,8,2,7,3);spr(7,8,5,1,1);spr(7,9,1,3,1);
        spr(7,20,10,4,2);spr(10,21,4,2,3);
    } else {
        for(int i=5;i<=17;i+=6){spr(i,1,2,4,2);spr(i,19,2,4,2);spr(1,i,4,2,2);spr(19,i,4,2,2);}
        spr(5,5,14,14,1);spr(7,7,10,10,4);spr(8,8,8,8,1);
        spr(9,10,1,2,4);spr(14,10,1,2,4);spr(10,14,4,1,4);
    }
}
static const uint8_t digits[10][7]={
    {14,17,17,17,17,17,14},{12,28,12,12,12,12,31},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};
/* A shared seven-row pixel alphabet draws both clock and date. No new font. */
static int pixel_text(lv_layer_t *layer,int x,int y,const char *text,int scale) {
    static const uint8_t letter_m[]={17,27,21,21,17,17,17};
    static const uint8_t letter_n[]={17,25,25,21,19,19,17};
    static const uint8_t letter_t[]={31,4,4,4,4,4,4};
    static const uint8_t letter_u[]={17,17,17,17,17,17,14};
    static const uint8_t letter_e[]={31,16,16,30,16,16,31};
    static const uint8_t letter_w[]={17,17,17,21,21,27,17};
    static const uint8_t letter_d[]={30,17,17,17,17,17,30};
    static const uint8_t letter_h[]={17,17,17,31,17,17,17};
    static const uint8_t letter_f[]={31,16,16,30,16,16,16};
    static const uint8_t letter_r[]={30,17,17,30,20,18,17};
    static const uint8_t letter_i[]={31,4,4,4,4,4,31};
    static const uint8_t letter_s[]={15,16,16,14,1,1,30};
    static const uint8_t letter_a[]={14,17,17,31,17,17,17};
    static const uint8_t letter_p[]={30,17,17,30,16,16,16};
    static const uint8_t dash[]={0,0,0,31,0,0,0};
    static const uint8_t colon[]={0,0,1,0,1,0,0};
    static const uint8_t dot[]={0,0,0,0,0,1,1};
    static const uint8_t slash[]={1,1,2,2,2,4,4};
    static const uint8_t space[]={0,0,0,0,0,0,0};
    int left=x;
    for(const char *c=text;*c;c++) {
        const uint8_t *rows=NULL;int width=5;
        if(*c>='0'&&*c<='9')rows=digits[*c-'0'];
        else if(*c=='M')rows=letter_m;
        else if(*c=='O')rows=digits[0];
        else if(*c=='N')rows=letter_n;
        else if(*c=='T')rows=letter_t;
        else if(*c=='U')rows=letter_u;
        else if(*c=='E')rows=letter_e;
        else if(*c=='W')rows=letter_w;
        else if(*c=='D')rows=letter_d;
        else if(*c=='H')rows=letter_h;
        else if(*c=='F')rows=letter_f;
        else if(*c=='R')rows=letter_r;
        else if(*c=='I')rows=letter_i;
        else if(*c=='S')rows=letter_s;
        else if(*c=='A')rows=letter_a;
        else if(*c=='P')rows=letter_p;
        else if(*c=='-')rows=dash;
        else if(*c==':'){rows=colon;width=1;}
        else if(*c=='.'){rows=dot;width=1;}
        else if(*c=='/'){rows=slash;width=3;}
        else if(*c==' '){rows=space;width=2;}
        if (!rows) return 0;
        for(int row=0;row<7;row++)for(int col=0;col<width;) {
        if(!(rows[row]&(1<<(width-1-col)))){col++;continue;}
        int first=col;
        while(col<width&&(rows[row]&(1<<(width-1-col))))col++;
        box(layer,x+first*scale,y+row*scale,(col-first)*scale,scale,INK);
        }
        x+=(width+1)*scale;
    }
    return x-left-scale;
}
static void art(lv_event_t *event) {
    if (!visible) return;
    lv_layer_t *layer=lv_event_get_layer(event);
    /* Frame inset fits the same rounded 32px safe area as the existing UI. */
    box(layer,24,12,192,1,EDGE);box(layer,24,307,192,1,EDGE);
    box(layer,12,24,1,272,EDGE);box(layer,227,24,1,272,EDGE);
    const int cx[]={16,219},cy[]={16,299};
    for(int i=0;i<2;i++)for(int j=0;j<2;j++) {
        box(layer,cx[i],cy[j]+(j?3:0),5,2,GOLD);
        box(layer,cx[i]+(i?3:0),cy[j],2,5,GOLD);
    }
    box(layer,20,49,70,2,GOLD);
    box(layer,160,27,17,10,EDGE);box(layer,177,30,2,4,EDGE);
    box(layer,162,29,13,6,BG);box(layer,162,29,(sample_battery < 0 ? 0 : sample_battery)*13/100,6,INK);
    box(layer,20,68+CONTENT_OFFSET_Y,64,64,PANEL);
    box(layer,24,68+CONTENT_OFFSET_Y,56,1,EDGE);box(layer,24,131+CONTENT_OFFSET_Y,56,1,EDGE);
    box(layer,20,72+CONTENT_OFFSET_Y,1,56,EDGE);box(layer,83,72+CONTENT_OFFSET_Y,1,56,EDGE);
    box(layer,20,68+CONTENT_OFFSET_Y,6,2,GOLD);box(layer,20,68+CONTENT_OFFSET_Y,2,6,GOLD);
    box(layer,78,130+CONTENT_OFFSET_Y,6,2,GOLD);box(layer,82,126+CONTENT_OFFSET_Y,2,6,GOLD);
    for(int y=0;y<24;y++)for(int x=0;x<24;) {
        uint8_t c=sprite[y][x];int start=x;
        while(x<24&&sprite[y][x]==c)x++;
        if(c)box(layer,28+start*2,76+y*2+CONTENT_OFFSET_Y,(x-start)*2,2,palette[c]);
    }
    box(layer,20,148+CONTENT_OFFSET_Y,92,1,EDGE);box(layer,128,148+CONTENT_OFFSET_Y,92,1,EDGE);
    box(layer,117,147+CONTENT_OFFSET_Y,2,2,MUTED);box(layer,123,147+CONTENT_OFFSET_Y,2,2,MUTED);
    pixel_text(layer,20,166+CONTENT_OFFSET_Y,clock_text,5);
    pixel_text(layer,160,185+CONTENT_OFFSET_Y,period_text,2);
    pixel_text(layer,20,214+CONTENT_OFFSET_Y,date_text,2);
    box(layer,20,FOOTER_Y+9,4,4,status_color);box(layer,112,FOOTER_Y+2,1,62,EDGE);
    int y=FOOTER_Y+selected*24;
    box(layer,122,y,98,23,GOLD);
    box(layer,208,y+7,2,2,BG);box(layer,210,y+9,2,2,BG);
    box(layer,212,y+11,2,2,BG);box(layer,210,y+13,2,2,BG);
    box(layer,208,y+15,2,2,BG);
}

void kp_home_ui_init(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, art, LV_EVENT_DRAW_MAIN_END, NULL);
}
void kp_home_ui_set(bool show, const kp_home_config_t *home, kp_ble_status_t ble,
                    int battery, unsigned menu, uint64_t epoch) {
    visible = show;
    if (!show) return;
    selected = menu < 3 ? menu : 0;
    sample_battery = battery < 0 ? -1 : battery > 100 ? 100 : battery;
    status_color = ble.ready && !ble.paused && !ble.failed ? GOLD : MUTED;
    make_sprite(home->avatar);
    kp_home_clock_text(home, epoch, clock_text, date_text, period_text);
}
