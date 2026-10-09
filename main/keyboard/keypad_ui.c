#include "keypad_ui.h"
#include "keypad_home_ui.h"
#include "bsp_display.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
LV_FONT_DECLARE(passport_font_20_menu);
LV_FONT_DECLARE(passport_font_28);
#define INK 0xe9ecdf
#define PAPER 0x18241f
#define ACCENT 0xd8a466
#define MENU_TOP 86
#define MENU_PITCH 42
#define MENU_HEIGHT 32
static lv_obj_t *s_title, *s_state, *s_battery, *s_panels[3], *s_keys[3], *s_lines[3][4];
static lv_obj_t *s_transfer_qr;
static char s_qr_url[40];
static lv_obj_t *s_body, *s_footer, *s_message, *s_profile_rows[4];
static lv_obj_t *s_command_title, *s_command_keys, *s_command_band, *s_command_status, *s_command_progress, *s_command_separator;
static lv_obj_t *s_screen_values[4], *s_screen_range, *s_screen_fill;
static const char *screen_names[] = {"正常亮度", "自动变暗", "变暗亮度", "自动息屏"};
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, w);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(INK), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, ""); return l;
}
static void hidden(lv_obj_t *object, bool hide) {
    if (hide) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}
static lv_obj_t *menu_row(unsigned index, bool selected) {
    lv_obj_t *row = s_profile_rows[index];
    hidden(row, false);
    lv_obj_set_pos(row, 20, MENU_TOP + index * MENU_PITCH);
    lv_obj_set_width(row, 200);
    lv_obj_set_height(row, MENU_HEIGHT);
    lv_obj_set_style_text_font(row, &passport_font_20_menu, 0);
    lv_obj_set_style_text_line_space(row, 0, 0);
    lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(row, lv_color_hex(selected ? ACCENT : INK), 0);
    return row;
}
void kp_ui_init(void) {
    if (!bsp_lvgl_lock(1000)) return;
    lv_obj_set_style_bg_color(lv_layer_bottom(), lv_color_hex(0x0f1c16), 0);
    lv_obj_set_style_bg_opa(lv_layer_bottom(), LV_OPA_COVER, 0);
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    // All children stay inside the rounded safe area; clip_corner would
    // allocate a full-width ARGB layer larger than the 24 KiB LVGL pool.
    lv_obj_set_style_radius(screen, 32, 0);
    lv_obj_set_style_border_width(screen, 0, 0); lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    kp_home_ui_init(screen);
    s_title = label(screen, 18, 20, 151, &passport_font_20);
    lv_obj_set_height(s_title, 24);
    s_battery = label(screen, 178, 24, 45, &passport_font_16);
    lv_obj_set_height(s_battery, passport_font_16.line_height);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_state = label(screen, 18, 46, 206, &passport_font_16);
    lv_obj_set_height(s_state, 22);
    lv_obj_set_style_text_color(s_state, lv_color_hex(ACCENT), 0);
    for (int i = 0; i < 3; ++i) {
        s_panels[i] = lv_obj_create(screen);
        lv_obj_set_pos(s_panels[i], 14, 46 + i * 84);
        lv_obj_set_size(s_panels[i], 212, 80);
        lv_obj_set_style_bg_color(s_panels[i], lv_color_hex(0x25362d), 0);
        lv_obj_set_style_bg_opa(s_panels[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_panels[i], 0, 0);
        lv_obj_set_style_radius(s_panels[i], 5, 0);
        lv_obj_set_style_pad_all(s_panels[i], 0, 0);
        lv_obj_remove_flag(s_panels[i], LV_OBJ_FLAG_SCROLLABLE);
        s_keys[i] = label(s_panels[i], 8, 29, 36, &passport_font_20);
        for (int j = 0; j < 4; ++j) {
            s_lines[i][j] = label(s_panels[i], 48, j * 20, 157, &passport_font_16);
            lv_obj_set_height(s_lines[i][j], 20);
            // Inline color keeps four rows within the existing 24 KiB LVGL pool.
            lv_label_set_recolor(s_lines[i][j], true);
            if (j == 2) {
                lv_obj_set_style_bg_color(s_lines[i][j], lv_color_hex(0x354a3c), 0);
                lv_obj_set_style_bg_opa(s_lines[i][j], LV_OPA_COVER, 0);
            }
        }
    }
    s_body = label(screen, 20, 83, 200, &passport_font_16);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_body, 2, 0);
    for (int i = 0; i < 4; ++i) { s_profile_rows[i] = label(screen, 20, MENU_TOP + i * MENU_PITCH, 200, &passport_font_20_menu); lv_obj_set_height(s_profile_rows[i], MENU_HEIGHT); }
    // Reuse the first control card's labels for settings columns, leaving RAM
    // for the font decoder and the large numeric glyph's temporary bitmap.
    for (int i = 0; i < 4; ++i) s_screen_values[i] = s_lines[0][i];
    s_message = label(screen, 18, 264, 204, &passport_font_16);
    lv_obj_set_height(s_message, 22);
    s_footer = label(screen, 18, 289, 204, &passport_font_16);
    lv_obj_set_height(s_footer, 22);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(0xa3b5a5), 0);
    s_command_band = lv_obj_create(screen);
    lv_obj_set_pos(s_command_band, 0, 86); lv_obj_set_size(s_command_band, 240, 94);
    lv_obj_set_style_bg_color(s_command_band, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(s_command_band, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_command_band, 0, 0);
    lv_obj_set_style_radius(s_command_band, 0, 0); lv_obj_set_style_pad_all(s_command_band, 0, 0);
    lv_obj_remove_flag(s_command_band, LV_OBJ_FLAG_SCROLLABLE);
    s_command_title = label(s_command_band, 18, 0, 204, &passport_font_28);
    lv_obj_set_style_text_color(s_command_title, lv_color_hex(PAPER), 0);
    // The body and footer are mutually exclusive with their detail-page roles.
    // Reuse them to leave room for compressed glyph decoding in the LVGL pool.
    s_command_keys = s_body;
    s_command_separator = lv_obj_create(screen);
    lv_obj_set_pos(s_command_separator, 18, 271); lv_obj_set_size(s_command_separator, 204, 1);
    lv_obj_set_style_bg_color(s_command_separator, lv_color_hex(0x617467), 0);
    lv_obj_set_style_bg_opa(s_command_separator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_command_separator, 0, 0);
    lv_obj_set_style_pad_all(s_command_separator, 0, 0);
    lv_obj_remove_flag(s_command_separator, LV_OBJ_FLAG_SCROLLABLE);
    s_screen_range = s_command_separator;
    s_screen_fill = lv_obj_create(s_screen_range);
    lv_obj_set_pos(s_screen_fill, 0, 0); lv_obj_set_height(s_screen_fill, 3);
    lv_obj_set_style_bg_color(s_screen_fill, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(s_screen_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen_fill, 0, 0); lv_obj_set_style_pad_all(s_screen_fill, 0, 0);
    lv_obj_set_style_radius(s_screen_fill, 0, 0); lv_obj_remove_flag(s_screen_fill, LV_OBJ_FLAG_SCROLLABLE);
    hidden(s_screen_fill, true);
    s_command_status = s_footer;
    s_command_progress = label(screen, 166, 288, 56, &passport_font_16);
    lv_obj_set_height(s_command_progress, 24);
    lv_obj_set_style_text_align(s_command_progress, LV_TEXT_ALIGN_RIGHT, 0);
    lv_screen_load(screen); bsp_lvgl_unlock();
}
void kp_ui_update(const kp_model_t *m, kp_ble_status_t ble, int battery,
                  const char *message, const char *ssid, const char *password,
                  const kp_home_config_t *home, uint64_t epoch) {
    static bool cached;
    static uint32_t revision;
    static kp_page_t previous_page;
    static unsigned previous_selected;
    static uint8_t previous_profile;
    static kp_feedback_t previous_feedback;
    static kp_ble_status_t previous_ble;
    static int previous_battery = -2;
    static char previous_message[96], previous_ssid[32], previous_password[32];
    static kp_home_config_t previous_home;
    static uint64_t previous_minute;
    static kp_transfer_view_t previous_transfer;
    static kp_lan_view_t previous_lan;
    if (cached && revision == m->config_revision && previous_profile == m->config.active_profile &&
        previous_page == m->page && previous_selected == m->selected &&
        !memcmp(&previous_transfer, &m->transfer, sizeof(m->transfer)) &&
        !memcmp(&previous_lan, &m->lan, sizeof(m->lan)) &&
        !memcmp(&previous_feedback, &m->feedback, sizeof(m->feedback)) && !memcmp(&previous_ble, &ble, sizeof(ble)) &&
        battery == previous_battery && !strcmp(previous_message, message) &&
        !strcmp(previous_ssid, ssid) && !strcmp(previous_password, password) &&
        !memcmp(&previous_home, home, sizeof(*home)) && (m->page != KP_MENU || previous_minute == epoch / 60)) return;
    if (!s_title || !bsp_lvgl_lock(1000)) return;
    previous_transfer = m->transfer;
    previous_lan = m->lan;
    if (s_transfer_qr && (m->page != KP_TRANSFER || m->transfer.state != KP_TRANSFER_READY)) {
        lv_obj_delete(s_transfer_qr); s_transfer_qr = NULL; s_qr_url[0] = 0;
    }
    cached = true; revision = m->config_revision; previous_profile = m->config.active_profile;
    previous_page = m->page; previous_selected = m->selected; previous_feedback = m->feedback;
    previous_ble = ble; previous_battery = battery;
    previous_home = *home; previous_minute = epoch / 60;
    bool home_page = m->page == KP_MENU;
    bool pairing_notice = ble.pairing_hint != KP_PAIRING_NONE && !ble.secure && !ble.paused;
    kp_home_ui_set(home_page, home, ble, battery, m->selected, epoch);
    lv_obj_invalidate(lv_screen_active());
    snprintf(previous_message, sizeof(previous_message), "%s", message);
    snprintf(previous_ssid, sizeof(previous_ssid), "%s", ssid);
    snprintf(previous_password, sizeof(previous_password), "%s", password);
    const char *titles[] = {"开始控制", "Passport", "设置", "通过本机热点", "清除配对", "情景列表", "屏幕", "屏幕", "配对系统", "文件流转", "Passport配置"};
    bool command = m->page == KP_CONSOLE && m->feedback.visible && !pairing_notice;
    hidden(s_title, false);
    lv_obj_set_pos(s_title, 18, 20); lv_obj_set_width(s_title, 151);
    lv_obj_set_pos(s_battery, 178, 24); lv_obj_set_width(s_battery, 45);
    lv_obj_set_style_text_font(s_title, command ? &passport_font_16 : &passport_font_20, 0);
    if (m->page == KP_SYSTEM || m->page == KP_TRANSFER) lv_obj_set_style_text_font(s_title, &passport_font_20_menu, 0);
    lv_label_set_text(s_title, command ? kp_text(&m->config, m->config.profiles[m->config.active_profile].name) : titles[m->page]);
    if (m->page == KP_HOTSPOT && m->lan.wifi) lv_label_set_text(s_title, "通过 WiFi");
    if (battery >= 0) lv_label_set_text_fmt(s_battery, "%d%%", battery);
    else lv_label_set_text(s_battery, "--");
    lv_label_set_text(s_state, ble.paused ? "蓝牙已暂停" : pairing_notice ? kp_pairing_title(ble.pairing_hint) :
                      ble.failed ? "蓝牙异常" : ble.ready ? "蓝牙已连接" :
                      ble.connected ? (!ble.handles_ready ? "键盘服务未就绪" :
                          !ble.secure ? "等待配对完成" : "等待电脑启用键盘") :
                      "等待电脑连接");
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *value = s_screen_values[i];
        bool system_value = m->page == KP_SETTINGS && i < 2;
        bool column = m->page == KP_SCREEN || system_value;
        lv_obj_t *parent = column ? lv_screen_active() : s_panels[0];
        if (lv_obj_get_parent(value) != parent) lv_obj_set_parent(value, parent);
        lv_obj_set_pos(value, column ? 156 : 48, column ? MENU_TOP + i * MENU_PITCH : i * 20);
        lv_obj_set_size(value, column ? 64 : 157, column ? MENU_HEIGHT : 20);
        lv_obj_set_style_text_font(value, column ? &passport_font_20_menu : &passport_font_16, 0);
        lv_obj_set_style_text_align(value, column ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_bg_opa(value, !column && i == 2 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_label_set_recolor(value, !column);
        if (system_value) {
            lv_obj_set_pos(value, 137, MENU_TOP + MENU_PITCH);
            lv_obj_set_size(value, 83, MENU_HEIGHT);
            lv_obj_set_style_text_font(value, &passport_font_16, 0);
        }
    }
    for (int i = 0; i < 3; ++i) {
        hidden(s_panels[i], true);
        // Hidden macro titles can otherwise retain almost 1 KiB while a large
        // command glyph needs its decode buffer. Refill on returning to control.
        if (m->page != KP_CONSOLE || command || pairing_notice)
            for (int j = 0; j < 4; ++j) lv_label_set_text(s_lines[i][j], "");
    }
    hidden(s_body, true);
    for (int i = 0; i < 4; ++i) {
        hidden(s_profile_rows[i], true); lv_label_set_text(s_profile_rows[i], "");
        lv_label_set_long_mode(s_profile_rows[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_line_space(s_profile_rows[i], 0, 0);
        hidden(s_screen_values[i], m->page != KP_CONSOLE || command); lv_label_set_text(s_screen_values[i], "");
    }
    hidden(s_screen_fill, true);
    hidden(s_message, m->page == KP_CONSOLE); hidden(s_footer, false);
    hidden(s_command_band, !command); hidden(s_command_title, !command); hidden(s_command_keys, !command);
    hidden(s_command_progress, !command); hidden(s_command_separator, !command);
    if (!command) lv_label_set_text(s_command_title, "");
    lv_obj_set_pos(s_command_band, 0, 86); lv_obj_set_size(s_command_band, 240, 94);
    lv_obj_set_style_text_font(s_command_title, &passport_font_28, 0);
    lv_obj_set_style_text_align(s_command_title, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(s_command_separator, 18, 271); lv_obj_set_size(s_command_separator, 204, 1);
    lv_obj_set_style_bg_color(s_command_separator, lv_color_hex(0x617467), 0);
    if (m->page == KP_CONSOLE && !command) lv_label_set_text(s_body, "");
    lv_obj_set_pos(s_command_progress, 166, 288); lv_obj_set_size(s_command_progress, 56, 24);
    lv_obj_set_style_text_color(s_command_progress, lv_color_hex(INK), 0);
    lv_obj_set_style_text_align(s_command_progress, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(s_message, &passport_font_16, 0);
    lv_obj_set_style_text_color(s_message, lv_color_hex(INK), 0);
    lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_letter_space(s_message, 0, 0);
    lv_obj_set_pos(s_message, 18, 264); lv_obj_set_size(s_message, 204, 22);
    lv_label_set_text(s_footer, "长按 OK 返回上一层");
    lv_obj_set_pos(s_footer, 18, 289);
    lv_obj_set_size(s_footer, 204, 22);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(0xa3b5a5), 0);
    lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(s_footer, &passport_font_16, 0);
    lv_label_set_long_mode(s_body, command ? LV_LABEL_LONG_DOT : LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_body, command ? 204 : 200);
    lv_obj_set_style_text_line_space(s_body, command ? 0 : 2, 0);
    lv_obj_set_height(s_body, command ? 52 : LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(s_body, &passport_font_16, 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(INK), 0);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(s_state, 18, 46);
    lv_obj_set_width(s_state, 206);
    lv_obj_set_height(s_state, 22);
    lv_obj_set_style_text_font(s_state, &passport_font_16, 0);
    lv_obj_set_style_text_color(s_state, lv_color_hex(ACCENT), 0);
    if (home_page) {
        // Reuse labels from mutually exclusive pages, leaving room for glyph
        // decoding in the existing 24 KiB pool. Art is drawn without layers.
        lv_obj_set_pos(s_title, 20, 20); lv_obj_set_width(s_title, 130);
        lv_obj_set_pos(s_battery, 180, 23); lv_obj_set_width(s_battery, 44);
        lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_t *user = s_command_progress;
        hidden(user, false); lv_obj_set_pos(user, 98, 59); lv_obj_set_size(user, 122, passport_font_16.line_height);
        lv_obj_set_style_text_align(user, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(user, lv_color_hex(0xa3b5a5), 0); lv_label_set_text(user, "USER");
        lv_obj_t *name = s_profile_rows[0], *badge = s_profile_rows[1];
        hidden(name, false); hidden(badge, false);
        lv_point_t size;
        lv_text_get_size(&size, home->name, &passport_font_20_menu, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const lv_font_t *font = size.x <= 122 ? &passport_font_20_menu : &passport_font_16;
        lv_obj_set_pos(name, 98, 79); lv_obj_set_size(name, 122, font->line_height);
        lv_obj_set_style_text_font(name, font, 0); lv_obj_set_style_text_color(name, lv_color_hex(INK), 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_label_set_text(name, home->name);
        lv_obj_set_pos(badge, 98, 109); lv_obj_set_size(badge, 122, passport_font_16.line_height);
        lv_obj_set_style_text_font(badge, &passport_font_16, 0); lv_obj_set_style_text_color(badge, lv_color_hex(0xa3b5a5), 0);
        lv_label_set_text(badge, home->badge);
        lv_obj_set_pos(s_state, 30, 233); lv_obj_set_size(s_state, 78, passport_font_16.line_height);
        lv_obj_set_style_text_font(s_state, &passport_font_16, 0); lv_obj_set_style_text_color(s_state, lv_color_hex(INK), 0);
        lv_label_set_text(s_state, ble.paused ? "已暂停" : pairing_notice ? "重新配对" : ble.failed ? "蓝牙异常" : ble.ready ? "已连接" :
            ble.connected ? (!ble.handles_ready ? "服务未就绪" : !ble.secure ? "等待配对" : "等待启用") : "等待连接");
        hidden(s_message, false); lv_obj_set_pos(s_message, 20, 257); lv_obj_set_size(s_message, 88, passport_font_16.line_height);
        lv_obj_set_style_text_font(s_message, &passport_font_16, 0);
        lv_obj_set_style_text_color(s_message, lv_color_hex(0xa3b5a5), 0); lv_label_set_text(s_message, "SCENE");
        lv_obj_set_pos(s_footer, 20, 277); lv_obj_set_size(s_footer, 88, passport_font_16.line_height);
        lv_obj_set_style_text_font(s_footer, &passport_font_16, 0); lv_obj_set_style_text_color(s_footer, lv_color_hex(INK), 0);
        lv_label_set_text(s_footer, kp_text(&m->config, m->config.profiles[m->config.active_profile].name));
        if (pairing_notice) {
            hidden(user, true); hidden(badge, true);
            lv_obj_set_pos(name, 20, 83); lv_obj_set_size(name, 200, 130);
            lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(name, &passport_font_16, 0);
            lv_obj_set_style_text_line_space(name, 2, 0);
            lv_label_set_text(name, kp_pairing_instructions(ble.pairing_hint));
        }
        const char *menu[] = {"开始控制", "情景列表", "设置"};
        lv_obj_t *rows[] = {s_profile_rows[2], s_profile_rows[3], s_body};
        for (unsigned i = 0; i < 3; ++i) {
            hidden(rows[i], false); lv_obj_set_pos(rows[i], 128, 233 + i * 24);
            lv_obj_set_size(rows[i], 76, passport_font_16.line_height);
            lv_obj_set_style_text_font(rows[i], &passport_font_16, 0);
            lv_obj_set_style_text_line_space(rows[i], 0, 0); lv_label_set_long_mode(rows[i], LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_color(rows[i], lv_color_hex(m->selected == i ? PAPER : INK), 0);
            lv_label_set_text(rows[i], menu[i]);
        }
    } else if (m->page == KP_TRANSFER) {
        const kp_transfer_view_t *t = &m->transfer;
        const char *states[] = {"已关闭", "未配置 Wi-Fi", "已开启 · 正在连接 Wi-Fi", "已开启 · Wi-Fi 已连接",
                                "已开启 · Wi-Fi 正在重连", "已开启 · 连接或服务失败", "正在关闭"};
        lv_label_set_text(s_state, states[t->state]);
        const char *choices[] = {"关闭", "开启"};
        for (unsigned i = 0; i < 2; ++i)
            lv_label_set_text_fmt(menu_row(i, m->selected == i), "%s %s%s", m->selected == i ? ">" : " ",
                choices[i], t->enabled == (i == 1) ? " *" : "");
        hidden(s_body, false); lv_obj_set_pos(s_body, 20, 186); lv_obj_set_size(s_body, 200, 68);
        lv_label_set_text(s_footer, "上下选择，OK 确认");
        lv_label_set_text(s_message, t->enabled ? "返回后仍保持开启" : "长按 OK 返回");
        if (t->state == KP_TRANSFER_READY && t->ip[0]) {
            char url[40]; snprintf(url, sizeof(url), "http://%s/", t->ip);
            if (!s_transfer_qr) {
                s_transfer_qr = lv_qrcode_create(lv_screen_active());
                lv_qrcode_set_size(s_transfer_qr, 99); lv_qrcode_set_quiet_zone(s_transfer_qr, true);
                lv_qrcode_set_dark_color(s_transfer_qr, lv_color_black());
                lv_qrcode_set_light_color(s_transfer_qr, lv_color_white());
                lv_obj_set_pos(s_transfer_qr, 20, 164);
            }
            if (strcmp(url, s_qr_url)) {
                if (lv_qrcode_update(s_transfer_qr, url, strlen(url)) == LV_RESULT_OK) snprintf(s_qr_url, sizeof(s_qr_url), "%s", url);
                else hidden(s_transfer_qr, true);
            }
            lv_obj_set_pos(s_body, 124, 167); lv_obj_set_size(s_body, 98, 47);
            lv_label_set_text(s_body, "扫码访问\nhttp://");
            lv_obj_t *ip = s_command_progress; hidden(ip, false);
            lv_obj_set_pos(ip, 124, 216); lv_obj_set_size(ip, 98, 19);
            lv_point_t ip_size;
            lv_text_get_size(&ip_size, t->ip, &passport_font_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            bool compact_ip = ip_size.x > 98;
            lv_obj_set_style_text_font(ip, compact_ip ? &lv_font_montserrat_14 : &passport_font_16, 0);
            lv_obj_set_style_text_letter_space(ip, compact_ip ? -1 : 0, 0);
            lv_obj_set_height(ip, compact_ip ? lv_font_montserrat_14.line_height : passport_font_16.line_height);
            lv_obj_set_style_text_align(ip, LV_TEXT_ALIGN_LEFT, 0); lv_label_set_text(ip, t->ip);
            lv_obj_t *count = s_profile_rows[2]; hidden(count, false);
            lv_obj_set_pos(count, 124, 238); lv_obj_set_size(count, 98, 24);
            lv_obj_set_style_text_font(count, &passport_font_16, 0); lv_obj_set_style_text_color(count, lv_color_hex(INK), 0);
            lv_label_set_text_fmt(count, "在线 %u 台", t->peers);
        } else if (t->state == KP_TRANSFER_NO_WIFI) lv_label_set_text(s_body, "请先通过本机热点\n保存 Wi-Fi 后再开启");
        else if (t->state == KP_TRANSFER_FAILED) lv_label_set_text(s_body, "请检查已保存的 Wi-Fi\n选择开启可重试连接");
        else if (t->state == KP_TRANSFER_CONNECTING) lv_label_set_text(s_body, "正在连接已保存的 Wi-Fi\n连接成功后显示访问地址");
        else if (t->state == KP_TRANSFER_RECONNECTING) lv_label_set_text(s_body, "连接已断开，正在重连\n重连后更新访问地址");
        else if (t->state == KP_TRANSFER_STOPPING) {lv_label_set_text(s_body, "正在结束共享会话\n蓝牙键盘可继续使用");lv_label_set_text(s_message, "请稍候");}
        else lv_label_set_text(s_body, "开启后，Wi-Fi 保持连接\n蓝牙键盘可继续使用\n文字、图片、文件直传");
    } else if (m->page == KP_SYSTEM) {
        lv_label_set_text_fmt(s_state, "当前：%s", m->system == KP_SYSTEM_MACOS ? "macOS" : "Windows");
        const char *systems[] = {"macOS", "Windows"};
        for (unsigned i = 0; i < KP_SYSTEM_COUNT; ++i) {
            lv_obj_t *row = menu_row(i, i == m->selected);
            lv_label_set_text_fmt(row, "%s %s%s", i == m->selected ? ">" : " ", systems[i], i == m->system ? " *" : "");
        }
        hidden(s_body, false); lv_obj_set_pos(s_body, 20, 195); lv_obj_set_size(s_body, 200, 24);
        lv_obj_set_style_text_color(s_body, lv_color_hex(0xa3b5a5), 0);
        lv_obj_set_style_text_line_space(s_body, 0, 0); lv_label_set_long_mode(s_body, LV_LABEL_LONG_DOT);
        lv_label_set_text_fmt(s_body, "执行共用 + %s", m->selected == KP_SYSTEM_MACOS ? "Mac" : "Windows");
        lv_label_set_text(s_message, m->system_saving ? "正在保存" : m->system_save_failed ? "保存失败，请重试" :
                          m->system_saved ? "已保存" : "长按 OK 返回");
        lv_obj_set_style_text_color(s_message, lv_color_hex(m->system_save_failed || m->system_saved ? ACCENT : 0xa3b5a5), 0);
        lv_label_set_text(s_footer, "上下选择，OK 保存");
    } else if (m->page == KP_SCREEN) {
        lv_label_set_text(s_state, "");
        for (unsigned i = 0; i < KP_SCREEN_SETTING_COUNT; ++i) {
            lv_obj_t *row = menu_row(i, i == m->selected);
            lv_obj_set_width(row, 132);
            lv_label_set_text_fmt(row, "%s %s", i == m->selected ? ">" : " ", screen_names[i]);
            lv_obj_t *value = s_screen_values[i]; hidden(value, false);
            lv_obj_set_style_text_color(value, lv_color_hex(i == m->selected ? ACCENT : INK), 0);
            unsigned number = kp_screen_value(&m->screen_config, i);
            bool brightness = i == KP_SCREEN_BRIGHTNESS || i == KP_SCREEN_DIM_BRIGHTNESS;
            if (!brightness && !number) lv_label_set_text(value, "关闭");
            else lv_label_set_text_fmt(value, brightness ? "%u%%" : "%u 秒", number);
        }
        hidden(s_message, true); lv_label_set_text(s_message, "");
        lv_label_set_text(s_footer, "上下选择，OK 调整");
    } else if (m->page == KP_SCREEN_EDIT) {
        lv_label_set_text(s_title, screen_names[m->screen_setting]);
        bool brightness = m->screen_setting == KP_SCREEN_BRIGHTNESS || m->screen_setting == KP_SCREEN_DIM_BRIGHTNESS;
        unsigned number = kp_screen_value(&m->screen_draft, m->screen_setting);
        if (m->screen_setting == KP_SCREEN_BRIGHTNESS) lv_label_set_text(s_state, "操作时使用的亮度");
        else if (m->screen_setting == KP_SCREEN_DIM_BRIGHTNESS) lv_label_set_text(s_state, "自动变暗后的亮度");
        else lv_label_set_text(s_state, "从最后一次操作起计时");
        hidden(s_command_band, false); hidden(s_command_title, false);
        lv_obj_set_pos(s_command_band, 0, 89); lv_obj_set_height(s_command_band, 85);
        lv_obj_set_style_text_align(s_command_title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(s_command_title, !brightness && !number ? &passport_font_28 : &lv_font_montserrat_48, 0);
        if (!brightness && !number) lv_label_set_text(s_command_title, "关闭");
        else lv_label_set_text_fmt(s_command_title, brightness ? "%u%%" : "%u", number);
        lv_obj_set_height(s_command_title, LV_SIZE_CONTENT); lv_obj_update_layout(s_command_title);
        lv_obj_set_y(s_command_title, (85 - lv_obj_get_height(s_command_title)) / 2);
        if (brightness) {
            unsigned min = m->screen_setting == KP_SCREEN_BRIGHTNESS ? 10 : 5;
            unsigned max = m->screen_setting == KP_SCREEN_BRIGHTNESS ? 100 : m->screen_draft.brightness;
            hidden(s_screen_range, false); hidden(s_screen_fill, false);
            lv_obj_set_pos(s_screen_range, 24, 195); lv_obj_set_size(s_screen_range, 192, 3);
            lv_obj_set_style_bg_color(s_screen_range, lv_color_hex(0x405344), 0);
            lv_obj_set_width(s_screen_fill, 192 * (number - min) / (max - min));
            for (unsigned i = 0; i < 2; ++i) {
                lv_obj_t *limit = s_profile_rows[i]; hidden(limit, false);
                lv_obj_set_pos(limit, i ? 156 : 24, 209); lv_obj_set_size(limit, 60, 24);
                lv_obj_set_style_text_font(limit, &passport_font_16, 0);
                lv_obj_set_style_text_align(limit, i ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
                lv_obj_set_style_text_color(limit, lv_color_hex(0xa3b5a5), 0);
                lv_label_set_text_fmt(limit, "%u%%", i ? max : min);
            }
        } else {
            hidden(s_body, false); lv_obj_set_pos(s_body, 18, 194); lv_obj_set_width(s_body, 204);
            lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(s_body, number ? "秒" : "保持当前亮度");
        }
        lv_obj_set_pos(s_message, 18, 242); lv_obj_set_height(s_message, 40);
        lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_message, lv_color_hex(0xa3b5a5), 0);
        lv_label_set_text(s_message, m->screen_save_failed ? "保存失败，请重试" :
            brightness ? "实时预览亮度" : "可设置为关闭");
        lv_label_set_text(s_footer, m->screen_saving ? "正在保存" : "上下调整，OK 保存");
    } else if (command) {
        const kp_binding_t *b = kp_binding(&m->config, m->feedback.slot);
        char title[128], sequence[512];
        kp_binding_title(&m->config, b, m->feedback.system, title, sizeof(title));
        kp_binding_label(&m->config, b, m->feedback.system, sequence, sizeof(sequence));
        const char *keys[] = {"上", "下", "OK"}; const char *gestures[] = {"单击", "双击", "三击", "长按"};
        lv_obj_set_pos(s_state, 18, 56);
        lv_obj_set_style_text_color(s_state, lv_color_hex(0xa3b5a5), 0);
        lv_label_set_text_fmt(s_state, "%s · %s", keys[m->feedback.slot / 4], gestures[m->feedback.slot % 4]);
        lv_label_set_text(s_command_title, title);
        // Use actual font metrics to center either one or two lines in the band.
        lv_obj_set_height(s_command_title, LV_SIZE_CONTENT); lv_obj_update_layout(s_command_title);
        int height = lv_obj_get_height(s_command_title), limit = passport_font_28.line_height * 2;
        if (height > limit) height = limit;
        lv_obj_set_height(s_command_title, height); lv_obj_set_y(s_command_title, (94 - height) / 2);
        lv_label_set_text(s_command_keys, sequence);
        lv_obj_set_pos(s_command_keys, 18, 204);
        lv_obj_set_pos(s_command_status, 18, 288);
        lv_obj_set_size(s_command_status, 146, 24);
        lv_obj_set_style_text_color(s_command_status, lv_color_hex(ACCENT), 0);
        lv_label_set_text(s_command_status, m->feedback.state == KP_FEEDBACK_FAILED ? "失败" :
            m->feedback.state == KP_FEEDBACK_DONE ? "已完成" : m->feedback.holding ? "保持按下" : "执行中");
        if (m->feedback.total > 1)
            lv_label_set_text_fmt(s_command_progress, "%u / %u", m->feedback.completed, m->feedback.total);
        else lv_label_set_text(s_command_progress, "");
    } else if (m->page == KP_CONSOLE) {
        hidden(s_title, true);
        lv_obj_set_pos(s_state, 18, 20); lv_obj_set_width(s_state, 151);
        if (!pairing_notice && ble.ready && !ble.paused && !ble.failed) lv_label_set_text_fmt(s_state, "%s · 已连接",
            kp_text(&m->config, m->config.profiles[m->config.active_profile].name));
        lv_obj_set_pos(s_footer, 18, 296);
        lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(s_footer, "长按 OK 返回");
        const char *keys[] = {"上", "下", "OK"}; const char *gestures[] = {"单", "双", "三", "长"};
        if (pairing_notice) {
            hidden(s_body, false); lv_obj_set_pos(s_body, 20, 83); lv_obj_set_size(s_body, 200, 160);
            lv_label_set_text(s_body, kp_pairing_instructions(ble.pairing_hint));
        }
        for (int i = 0; i < 3 && !pairing_notice; ++i) {
            hidden(s_panels[i], false); lv_label_set_text(s_keys[i], keys[i]);
            for (int j = 0; j < 4; ++j) {
                char action[128], escaped[256], line[288];
                const kp_binding_t *b = kp_binding(&m->config, i * 4 + j);
                bool fixed = i == 2 && j == 3;
                if (fixed) snprintf(action, sizeof(action), "返回");
                else kp_binding_title(&m->config, b, m->system, action, sizeof(action));
                // Literal '#' in user titles must not become a color command.
                unsigned n = 0;
                for (unsigned k = 0; action[k]; ++k) {
                    if (action[k] == '#') escaped[n++] = '#';
                    escaped[n++] = action[k];
                }
                escaped[n] = 0;
                lv_obj_set_style_text_color(s_lines[i][j], lv_color_hex(fixed || (b && b->count) ? INK : 0x829785), 0);
                snprintf(line, sizeof(line), "#%s %s#  %s", j == 2 ? "d8a466" : "a3b5a5", gestures[j], escaped);
                lv_label_set_text(s_lines[i][j], line);
            }
        }
    } else {
        hidden(s_body, false); lv_obj_set_pos(s_body, 20, 83); char text[768] = {0};
        if (m->page == KP_PROFILES) {
            hidden(s_body, true);
            unsigned first = m->selected >= 4 ? m->selected - 3 : 0;
            for (unsigned i = first; i < m->config.profile_count && i < first + 4; ++i) {
                lv_obj_t *row = menu_row(i - first, i == m->selected);
                lv_label_set_text_fmt(row, "%s %s%s", i == m->selected ? ">" : " ",
                         kp_text(&m->config, m->config.profiles[i].name), i == m->config.active_profile ? " *" : "");
            }
            lv_label_set_text(s_footer, "上下选择，OK 切换");
        } else if (m->page == KP_MENU || m->page == KP_SETTINGS || m->page == KP_FORGET || m->page == KP_LAN) {
            hidden(s_body, true);
            const char *menu[] = {"开始控制", "情景列表", "设置"};
            const char *settings[] = {"屏幕", "Passport配置", "文件流转", "配对系统", "清除蓝牙配对"};
            const char *lan[] = {"通过本机热点", "通过 WiFi"};
            const char *forget[] = {"取消", "确认清除"};
            const char **entries = m->page == KP_MENU ? menu : m->page == KP_SETTINGS ? settings : m->page == KP_LAN ? lan : forget;
            unsigned count = m->page == KP_SETTINGS ? KP_SETTINGS_COUNT : m->page == KP_MENU ? 3 : 2;
            unsigned first = m->page == KP_SETTINGS ? kp_settings_first(m->selected) : 0;
            for (unsigned i = first; i < count && i < first + 4; ++i) {
                lv_obj_t *row = menu_row(i - first, i == m->selected);
                lv_label_set_text_fmt(row, "%s %s", i == m->selected ? ">" : " ", entries[i]);
                if (m->page == KP_SETTINGS && (i == 2 || i == 3)) {
                    lv_obj_set_width(row, 113);
                    lv_obj_t *value = s_screen_values[i - 2]; hidden(value, false);
                    lv_obj_set_pos(value, 137, MENU_TOP + (i - first) * MENU_PITCH); lv_obj_set_size(value, 83, MENU_HEIGHT);
                    lv_obj_set_style_text_color(value, lv_color_hex(m->selected == i ? ACCENT : INK), 0);
                    lv_label_set_text(value, i == 2 ? (m->transfer.enabled ? "已开启" : "已关闭") :
                        m->system == KP_SYSTEM_MACOS ? "macOS" : "Windows");
                }
            }
            if (m->page == KP_FORGET) {
                hidden(s_body, false); lv_obj_set_pos(s_body, 20, 195);
                snprintf(text, sizeof(text), "清除后请在电脑忽略设备\n再重新连接");
                lv_label_set_text(s_footer, "上下选择，OK 确认");
            } else lv_label_set_text(s_footer, "上下选择，OK 进入");
        } else if (m->page == KP_HOTSPOT) {
            if (m->lan.state == KP_AP_ACTIVE) {
                if (m->lan.wifi) snprintf(text, sizeof(text), "已连接 WiFi\n%s\n浏览器打开\nhttp://%s\n请连接同一局域网", m->lan.ssid, m->lan.ip);
                else snprintf(text, sizeof(text), "连接本机热点\n%s\n密码\n%s\n浏览器打开\nhttp://192.168.4.1", ssid, password);
            } else if (m->lan.state == KP_AP_NO_WIFI) snprintf(text, sizeof(text), "尚未配置 WiFi\n先通过本机热点保存网络");
            else if (m->lan.state == KP_AP_FAILED) snprintf(text, sizeof(text), "%s\n长按 OK 返回后重试", m->lan.wifi ? "WiFi 连接或服务启动失败" : "本机热点操作失败");
            else snprintf(text, sizeof(text), "%s\n请稍候，长按 OK 取消", m->lan.state == KP_AP_STOPPING ? "正在关闭配置" : m->lan.wifi ? "正在连接 WiFi" : "正在开启本机热点");
            lv_label_set_text(s_footer, "长按 OK 关闭并返回");
        }
        lv_label_set_text(s_body, text);
    }
    if (!home_page && m->page != KP_SCREEN && m->page != KP_SCREEN_EDIT && m->page != KP_SYSTEM && m->page != KP_TRANSFER) lv_label_set_text(s_message, message);
    if (m->page == KP_LAN) lv_label_set_text(s_message, "");
    if (m->page == KP_HOTSPOT && !m->lan.wifi) {
        lv_obj_set_style_text_letter_space(s_message, -1, 0);
        lv_label_set_text(s_message, "开启本机热点会暂时断开蓝牙");
    }
    if (!home_page) lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    bsp_lvgl_unlock();
}
