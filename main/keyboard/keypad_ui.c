#include "keypad_ui.h"
#include "bsp_display.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
LV_FONT_DECLARE(passport_font_28);
#define INK 0xe9ecdf
#define PAPER 0x18241f
#define ACCENT 0xd8a466
static lv_obj_t *s_title, *s_state, *s_battery, *s_panels[3], *s_keys[3], *s_lines[3][3];
static lv_obj_t *s_body, *s_footer, *s_message, *s_profile_rows[4];
static lv_obj_t *s_command_title, *s_command_keys, *s_command_description, *s_command_progress;
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
void kp_ui_init(void) {
    if (!bsp_lvgl_lock(1000)) return;
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    s_title = label(screen, 18, 20, 151, &passport_font_20);
    lv_obj_set_height(s_title, 24);
    s_battery = label(screen, 178, 24, 45, &passport_font_16);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_state = label(screen, 18, 46, 206, &passport_font_16);
    lv_obj_set_height(s_state, 22);
    lv_obj_set_style_text_color(s_state, lv_color_hex(ACCENT), 0);
    for (int i = 0; i < 3; ++i) {
        s_panels[i] = lv_obj_create(screen);
        lv_obj_set_pos(s_panels[i], 14, 73 + i * 62);
        lv_obj_set_size(s_panels[i], 212, 60);
        lv_obj_set_style_bg_color(s_panels[i], lv_color_hex(0x25362d), 0);
        lv_obj_set_style_bg_opa(s_panels[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_panels[i], 0, 0);
        lv_obj_set_style_radius(s_panels[i], 5, 0);
        lv_obj_set_style_pad_all(s_panels[i], 0, 0);
        lv_obj_remove_flag(s_panels[i], LV_OBJ_FLAG_SCROLLABLE);
        s_keys[i] = label(s_panels[i], 8, 19, 36, &passport_font_20);
        for (int j = 0; j < 3; ++j) {
            s_lines[i][j] = label(s_panels[i], 48, j * 20, 157, &passport_font_16);
            lv_obj_set_height(s_lines[i][j], 20);
        }
    }
    s_body = label(screen, 20, 83, 200, &passport_font_16);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_body, 2, 0);
    for (int i = 0; i < 4; ++i) { s_profile_rows[i] = label(screen, 20, 86 + i * 42, 200, &passport_font_16); lv_obj_set_height(s_profile_rows[i], 30); }
    s_message = label(screen, 18, 264, 204, &passport_font_16);
    lv_obj_set_height(s_message, 22);
    s_footer = label(screen, 18, 289, 204, &passport_font_16);
    lv_obj_set_height(s_footer, 22);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(0xa3b5a5), 0);
    s_command_title = label(screen, 18, 76, 204, &passport_font_28);
    lv_obj_set_height(s_command_title, 84);
    s_command_keys = label(screen, 18, 168, 204, &passport_font_16);
    lv_obj_set_height(s_command_keys, 48);
    lv_obj_set_style_text_color(s_command_keys, lv_color_hex(ACCENT), 0);
    s_command_description = label(screen, 18, 222, 204, &passport_font_16);
    lv_obj_set_height(s_command_description, 58);
    s_command_progress = label(screen, 18, 290, 204, &passport_font_16);
    lv_screen_load(screen); bsp_lvgl_unlock();
}
void kp_ui_update(const kp_model_t *m, kp_ble_status_t ble, int battery,
                  const char *message, const char *ssid, const char *password) {
    static bool cached;
    static uint32_t revision;
    static kp_page_t previous_page;
    static unsigned previous_selected;
    static uint8_t previous_profile;
    static kp_feedback_t previous_feedback;
    static kp_ble_status_t previous_ble;
    static int previous_battery = -2;
    static char previous_message[96], previous_ssid[32], previous_password[32];
    if (cached && revision == m->config_revision && previous_profile == m->config.active_profile &&
        previous_page == m->page && previous_selected == m->selected &&
        !memcmp(&previous_feedback, &m->feedback, sizeof(m->feedback)) && !memcmp(&previous_ble, &ble, sizeof(ble)) &&
        battery == previous_battery && !strcmp(previous_message, message) &&
        !strcmp(previous_ssid, ssid) && !strcmp(previous_password, password)) return;
    if (!s_title || !bsp_lvgl_lock(1000)) return;
    cached = true; revision = m->config_revision; previous_profile = m->config.active_profile;
    previous_page = m->page; previous_selected = m->selected; previous_feedback = m->feedback;
    previous_ble = ble; previous_battery = battery;
    snprintf(previous_message, sizeof(previous_message), "%s", message);
    snprintf(previous_ssid, sizeof(previous_ssid), "%s", ssid);
    snprintf(previous_password, sizeof(previous_password), "%s", password);
    const char *titles[] = {"开始控制", "Passport", "设置", "热点配置", "清除配对", "情景列表"};
    bool command = m->page == KP_CONSOLE && m->feedback.visible;
    lv_obj_set_style_text_font(s_title, command ? &passport_font_16 : &passport_font_20, 0);
    lv_label_set_text(s_title, command ? kp_text(&m->config, m->config.profiles[m->config.active_profile].name) : titles[m->page]);
    if (battery >= 0) lv_label_set_text_fmt(s_battery, "%d%%", battery);
    else lv_label_set_text(s_battery, "--");
    lv_label_set_text(s_state, ble.paused ? "蓝牙已暂停" : ble.ready ? "蓝牙已连接" :
                      ble.connected ? (!ble.handles_ready ? "键盘服务未就绪" :
                          !ble.secure ? "等待配对完成" : "等待电脑启用键盘") :
                      ble.failed ? "蓝牙异常" : "等待电脑连接");
    for (int i = 0; i < 3; ++i) hidden(s_panels[i], true);
    hidden(s_body, true);
    for (int i = 0; i < 4; ++i) hidden(s_profile_rows[i], true);
    hidden(s_message, command); hidden(s_footer, command);
    hidden(s_command_title, !command); hidden(s_command_keys, !command);
    hidden(s_command_description, !command); hidden(s_command_progress, !command);
    lv_label_set_text(s_footer, "长按 OK 返回上一层");
    if (command) {
        const kp_binding_t *b = kp_binding(&m->config, m->feedback.slot);
        char title[128], sequence[512];
        kp_binding_title(&m->config, b, title, sizeof(title));
        kp_binding_label(&m->config, b, sequence, sizeof(sequence));
        lv_label_set_text(s_command_title, title); lv_label_set_text(s_command_keys, sequence);
        lv_label_set_text(s_command_description, kp_text(&m->config, b->description));
        if (m->feedback.state == KP_FEEDBACK_RUNNING)
            lv_label_set_text_fmt(s_command_progress, m->feedback.holding ? "保持按下，松开释放" : "执行中 %u / %u", m->feedback.completed, m->feedback.total);
        else if (m->feedback.state == KP_FEEDBACK_FAILED)
            lv_label_set_text_fmt(s_command_progress, "失败 %u / %u，已停止", m->feedback.completed, m->feedback.total);
        else lv_label_set_text(s_command_progress, "已完成");
    } else if (m->page == KP_CONSOLE) {
        lv_label_set_text_fmt(s_state, "%s · %s", kp_text(&m->config, m->config.profiles[m->config.active_profile].name),
                              ble.ready ? "已连接" : "等待连接");
        const char *keys[] = {"上", "下", "OK"}; const char *gestures[] = {"单", "双", "长"};
        for (int i = 0; i < 3; ++i) {
            hidden(s_panels[i], false); lv_label_set_text(s_keys[i], keys[i]);
            for (int j = 0; j < 3; ++j) {
                char action[128], line[144];
                if (i == 2 && j == 2) snprintf(action, sizeof(action), "返回");
                else kp_binding_title(&m->config, kp_binding(&m->config, i * 3 + j), action, sizeof(action));
                snprintf(line, sizeof(line), "%s  %s", gestures[j], action); lv_label_set_text(s_lines[i][j], line);
            }
        }
    } else {
        hidden(s_body, false); char text[768] = {0};
        if (m->page == KP_PROFILES) {
            hidden(s_body, true);
            unsigned first = m->selected >= 4 ? m->selected - 3 : 0;
            for (unsigned i = first; i < m->config.profile_count && i < first + 4; ++i) {
                lv_obj_t *row = s_profile_rows[i - first]; hidden(row, false);
                lv_label_set_text_fmt(row, "%s %s%s", i == m->selected ? ">" : " ",
                         kp_text(&m->config, m->config.profiles[i].name), i == m->config.active_profile ? " *" : "");
                lv_obj_set_style_text_color(row, lv_color_hex(i == m->selected ? ACCENT : INK), 0);
            }
            lv_label_set_text(s_footer, "上下选择，OK 切换");
        } else if (m->page == KP_MENU) {
            const char *entries[] = {"开始控制", "情景列表", "设置"};
            snprintf(text, sizeof(text), "%s %s\n\n%s %s\n\n%s %s\n\n上下选择，OK 进入",
                     m->selected == 0 ? ">" : " ", entries[0], m->selected == 1 ? ">" : " ", entries[1],
                     m->selected == 2 ? ">" : " ", entries[2]);
            lv_label_set_text(s_footer, "OK 进入所选项目");
        } else if (m->page == KP_SETTINGS || m->page == KP_FORGET) {
            const char *first = m->page == KP_SETTINGS ? "热点配置" : "取消";
            const char *second = m->page == KP_SETTINGS ? "清除蓝牙配对" : "确认清除";
            snprintf(text, sizeof(text), "%s %s\n\n%s %s\n\n%s", m->selected == 0 ? ">" : " ", first,
                     m->selected == 1 ? ">" : " ", second,
                     m->page == KP_FORGET ? "清除后请在电脑忽略设备\n再重新连接" : "上下选择，OK 进入");
        } else if (m->page == KP_HOTSPOT) {
            snprintf(text, sizeof(text), "连接此热点\n%s\n密码\n%s\n浏览器打开\nhttp://192.168.4.1", ssid, password);
        }
        lv_label_set_text(s_body, text);
    }
    lv_label_set_text(s_message, message); bsp_lvgl_unlock();
}
