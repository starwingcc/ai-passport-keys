#include "keypad_ui.h"
#include "bsp_display.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
#define INK 0xe9ecdf
#define PAPER 0x18241f
#define ACCENT 0xd8a466
static lv_obj_t *s_title, *s_state, *s_battery, *s_panels[3], *s_keys[3], *s_lines[3][3];
static lv_obj_t *s_body, *s_footer, *s_message;
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, w);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(INK), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "");
    return l;
}
void kp_ui_init(void) {
    if (!bsp_lvgl_lock(1000)) return;
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    s_title = label(screen, 18, 20, 151, &passport_font_20);
    s_battery = label(screen, 178, 24, 45, &passport_font_16);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_state = label(screen, 18, 46, 206, &passport_font_16);
    lv_obj_set_style_text_color(s_state, lv_color_hex(ACCENT), 0);
    for (int i = 0; i < 3; ++i) {
        s_panels[i] = lv_obj_create(screen);
        lv_obj_set_pos(s_panels[i], 14, 73 + i * 62);
        lv_obj_set_size(s_panels[i], 212, 60);
        lv_obj_set_style_bg_color(s_panels[i], lv_color_hex(0x25362d), 0);
        lv_obj_set_style_border_width(s_panels[i], 0, 0);
        lv_obj_set_style_radius(s_panels[i], 5, 0);
        lv_obj_set_style_pad_all(s_panels[i], 0, 0);
        lv_obj_remove_flag(s_panels[i], LV_OBJ_FLAG_SCROLLABLE);
        s_keys[i] = label(s_panels[i], 8, 19, 36, &passport_font_20);
        for (int j = 0; j < 3; ++j) s_lines[i][j] = label(s_panels[i], 48, j * 20, 157, &passport_font_16);
    }
    s_body = label(screen, 20, 83, 200, &passport_font_16);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_body, 2, 0);
    s_message = label(screen, 18, 264, 204, &passport_font_16);
    s_footer = label(screen, 18, 289, 204, &passport_font_16);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(0xa3b5a5), 0);
    lv_screen_load(screen);
    bsp_lvgl_unlock();
}
void kp_ui_update(const kp_model_t *m, kp_ble_status_t ble, int battery,
                  const char *message, const char *ssid, const char *password) {
    // Gesture timestamps change while a key is held; only visible state
    // belongs in the render cache.
    static bool cached;
    static kp_config_t previous_config;
    static kp_page_t previous_page;
    static unsigned previous_selected;
    static kp_ble_status_t previous_ble;
    static int previous_battery = -2;
    static char previous_message[96], previous_ssid[32], previous_password[32];
    if (cached && previous_page == m->page && previous_selected == m->selected &&
        !memcmp(&previous_config, &m->config, sizeof(m->config)) &&
        !memcmp(&previous_ble, &ble, sizeof(ble)) &&
        battery == previous_battery && !strcmp(previous_message, message) &&
        !strcmp(previous_ssid, ssid) && !strcmp(previous_password, password)) return;
    if (!s_title || !bsp_lvgl_lock(1000)) return;
    cached = true;
    previous_config = m->config;
    previous_page = m->page;
    previous_selected = m->selected;
    previous_ble = ble;
    previous_battery = battery;
    snprintf(previous_message, sizeof(previous_message), "%s", message);
    snprintf(previous_ssid, sizeof(previous_ssid), "%s", ssid);
    snprintf(previous_password, sizeof(previous_password), "%s", password);
    const char *titles[] = {"指令控制台", "Passport", "设置", "热点配置", "清除配对"};
    lv_label_set_text(s_title, titles[m->page]);
    if (battery >= 0) lv_label_set_text_fmt(s_battery, "%d%%", battery);
    else lv_label_set_text(s_battery, "--");
    lv_label_set_text(s_state, ble.paused ? "蓝牙已暂停" : ble.ready ? "蓝牙已连接" :
                      ble.connected ? (!ble.handles_ready ? "键盘服务未就绪" :
                          !ble.secure ? "等待配对完成" : "等待电脑启用键盘") :
                      ble.failed ? "蓝牙异常" : "等待电脑连接");
    for (int i = 0; i < 3; ++i) lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_body, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_footer, "长按 OK 返回上一层");
    if (m->page == KP_CONSOLE) {
        const char *keys[] = {"上", "下", "OK"};
        const char *gestures[] = {"单", "双", "长"};
        for (int i = 0; i < 3; ++i) {
            lv_obj_remove_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_keys[i], keys[i]);
            for (int j = 0; j < 3; ++j) {
                char action[80], line[96];
                if (i == 2 && j == 2) snprintf(action, sizeof(action), "返回");
                else kp_action_label(&m->config.bindings[i * 3 + j], action, sizeof(action));
                snprintf(line, sizeof(line), "%s  %s", gestures[j], action);
                lv_label_set_text(s_lines[i][j], line);
            }
        }
    } else {
        lv_obj_remove_flag(s_body, LV_OBJ_FLAG_HIDDEN);
        char text[384] = {0};
        if (m->page == KP_MENU || m->page == KP_SETTINGS || m->page == KP_FORGET) {
            const char *first = m->page == KP_MENU ? "指令模式" : m->page == KP_SETTINGS ? "热点配置" : "取消";
            const char *second = m->page == KP_MENU ? "设置" : m->page == KP_SETTINGS ? "清除蓝牙配对" : "确认清除";
            snprintf(text, sizeof(text), "%s %s\n\n%s %s\n\n%s", m->selected == 0 ? ">" : " ", first,
                     m->selected == 1 ? ">" : " ", second,
                     m->page == KP_FORGET ? "清除后请在电脑忽略设备\n再重新连接" : "上下选择，OK 进入");
            if (m->page == KP_MENU) lv_label_set_text(s_footer, "OK 进入所选模式");
        } else if (m->page == KP_HOTSPOT) {
            snprintf(text, sizeof(text), "连接此热点\n%s\n\n密码\n%s\n\n浏览器打开\nhttp://192.168.4.1", ssid, password);
        }
        lv_label_set_text(s_body, text);
    }
    lv_label_set_text(s_message, message);
    bsp_lvgl_unlock();
}
