#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "keypad_model.h"
#include "keypad_ble.h"
#include "keypad_store.h"
#include "keypad_web.h"
#include "keypad_ui.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned key; bool down; uint32_t at; } input_t;
static QueueHandle_t s_queue;
static atomic_uint s_physical, s_requests;
static atomic_bool s_overflow;
static kp_model_t s_model;
static char s_message[96] = "准备连接 Passport Keys";
static bool s_pulse, s_ap_start, s_ap_stop, s_forget, s_ap_active;
static uint32_t s_pulse_until;
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void message(const char *text) { snprintf(s_message, sizeof(s_message), "%s", text); }
static void release(void) {
    s_pulse = false;
    if (kp_ble_status().ready && kp_ble_send(NULL) != ESP_OK) {
        kp_ble_recover(); message("发送异常，正在重连");
    }
}
static void effect(void *user, kp_effect_t event, const kp_action_t *action) {
    (void)user;
    switch (event) {
    case KP_TAP: case KP_HOLD:
        kp_ble_status_t status = kp_ble_status();
        if (!status.ready) {
            message(!status.connected ? "尚未连接，指令已忽略" :
                    !status.handles_ready ? "键盘服务未就绪" :
                    !status.secure ? "等待配对完成" : "等待电脑启用键盘");
            ESP_LOGW("keypad", "Input skipped: connected=%d secure=%d handles=%d keyboard_notify=%d paused=%d",
                     status.connected, status.secure, status.handles_ready,
                     status.keyboard_subscribed, status.paused);
            break;
        }
        release();
        esp_err_t sent = kp_ble_send(action);
        if (sent == ESP_ERR_NOT_SUPPORTED) { message("电脑未启用媒体按键"); break; }
        if (sent != ESP_OK) { kp_ble_recover(); message("发送异常，正在重连"); break; }
        kp_action_label(action, s_message, sizeof(s_message));
        s_pulse = event == KP_TAP;
        s_pulse_until = now_ms() + 40;
        break;
    case KP_RELEASE: release(); break;
    case KP_AP_START: s_ap_start = true; break;
    case KP_AP_STOP: s_ap_stop = true; break;
    case KP_CLEAR_BOND: s_forget = true; break;
    }
}
static void on_key(bsp_btn_t key, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_RELEASE) return;
    bool down = ev == BSP_BTN_PRESS;
    if (down) atomic_fetch_or(&s_physical, 1U << key);
    else atomic_fetch_and(&s_physical, ~(1U << key));
    input_t input = {.key = key, .down = down, .at = now_ms()};
    if (xQueueSend(s_queue, &input, 0) != pdTRUE) atomic_store(&s_overflow, true);
}
static void web_request(bool forget) { atomic_fetch_or(&s_requests, forget ? 2U : 1U); }
static void resync_input(void) {
    // Drop all queued events on mode/link transitions. Synchronize the physical
    // state as well, including a release that could have been lost on overflow.
    xQueueReset(s_queue);
    s_model.down_mask = atomic_load(&s_physical);
    kp_cancel(&s_model);
}
static void memory_log(const char *phase) {
    ESP_LOGI("keypad", "%s free=%u largest=%u min=%u stack=%u", phase,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
void app_main(void) {
    esp_err_t rc = nvs_flash_init();
    if (rc != ESP_OK) { ESP_LOGE("keypad", "NVS unavailable: %s; not erasing user data", esp_err_to_name(rc)); return; }
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(kp_store_init());
    kp_config_t config; kp_store_get(&config);
    kp_init(&s_model, &config, effect, NULL);
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) { ESP_LOGE("keypad", "Display unavailable"); return; }
    bsp_display_backlight(80);
    bool battery_ok = bsp_battery_init() == ESP_OK;
    kp_ui_init();
    s_queue = xQueueCreate(24, sizeof(input_t));
    if (!s_queue) { ESP_LOGE("keypad", "Input queue allocation failed"); return; }
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    rc = kp_ble_init();
    if (rc != ESP_OK) message("蓝牙启动失败，请重启");
    memory_log("startup");
    uint32_t last_ui = 0, last_battery = 0, last_activity = now_ms(), generation = 0;
    bool dimmed = false;
    int battery = -1;
    for (;;) {
        uint32_t now = now_ms();
        kp_ble_status_t ble = kp_ble_status();
        if (ble.generation != generation) {
            generation = ble.generation; resync_input();
        }
        if (atomic_exchange(&s_overflow, false)) { resync_input(); message("输入已恢复，请重新按键"); }
        input_t input;
        if (xQueueReceive(s_queue, &input, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Consume timestamped releases before advancing the hold timer to
            // wall time, so a delayed worker cannot invent a long press.
            do {
                last_activity = now_ms();
                if ((uint32_t)(last_activity - input.at) > 250) { resync_input(); break; }
                kp_input(&s_model, input.key, input.down, input.at);
            } while (xQueueReceive(s_queue, &input, 0) == pdTRUE);
        }
        now = now_ms();
        kp_tick(&s_model, now);
        if (s_pulse && (int32_t)(now - s_pulse_until) >= 0) release();
        unsigned request = atomic_exchange(&s_requests, 0);
        if (request && s_model.page == KP_HOTSPOT) kp_hotspot_exit(&s_model);
        if (s_ap_stop) {
            s_ap_stop = false;
            rc = kp_web_stop();
            if (rc == ESP_OK) {
                s_ap_active = false; kp_store_get(&s_model.config);
                kp_ble_pause(false); message("热点已关闭");
                if (request & 2U) { s_model.page = KP_FORGET; s_model.selected = 0; }
            } else { s_model.page = KP_HOTSPOT; message("关闭失败，长按 OK 重试"); }
            resync_input(); memory_log("AP stop");
        }
        if (s_ap_start) {
            s_ap_start = false; resync_input();
            message("正在开启热点");
            kp_ui_update(&s_model, kp_ble_status(), battery, s_message, "...", "...");
            rc = kp_ble_pause(true);
            if (rc == ESP_OK) rc = kp_web_start(web_request);
            s_ap_active = rc == ESP_OK;
            if (!s_ap_active) {
                if (kp_web_stop() == ESP_OK) kp_ble_pause(false);
                message("热点启动失败，长按返回");
            }
            else message("配置完成后长按 OK 返回");
            resync_input(); memory_log("AP start");
        }
        if (s_forget) {
            s_forget = false; resync_input();
            message(kp_ble_forget() == ESP_OK ? "配对已清除，请重新连接" : "清除失败，请重试");
            resync_input();
        }
        bool dim = (uint32_t)(now - last_activity) >= 60000;
        if (dim != dimmed) { dimmed = dim; bsp_display_backlight(dim ? 12 : 80); }
        if (battery_ok && (!last_battery || (uint32_t)(now - last_battery) >= 30000)) {
            battery = bsp_battery_soc(); last_battery = now;
        }
        if ((uint32_t)(now - last_ui) >= 100) {
            kp_ble_poll();
            kp_ui_update(&s_model, kp_ble_status(), battery, s_message,
                         s_ap_active ? kp_web_ssid() : "未开启", s_ap_active ? kp_web_password() : "--");
            last_ui = now;
        }
    }
}
