#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "keypad_model.h"
#include "keypad_ble.h"
#include "keypad_store.h"
#include "keypad_ui.h"
#include "keypad_runner.h"
#include "keypad_network_service.h"
#include <stdlib.h>
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
static atomic_uint s_physical;
static atomic_bool s_overflow;
static kp_model_t s_model;
static char s_message[96] = "准备连接 Passport Keys";
static bool s_forget, s_ap_active, s_select_profile;
static bool s_save_screen, s_save_system;
static unsigned s_target_profile;
static kp_runner_t s_runner;
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void message(const char *text) { snprintf(s_message, sizeof(s_message), "%s", text); }
static void release(void) {
    if (kp_ble_status().ready && kp_ble_send(NULL) != ESP_OK) {
        kp_ble_recover(); message("发送异常，正在重连");
    }
}
static void effect(void *user, kp_effect_t event, const kp_action_t *action) {
    (void)user;
    switch (event) {
    case KP_TAP: case KP_HOLD: {
        (void)action;
        kp_ble_status_t status = kp_ble_status();
        if (!status.ready) {
            message(!status.connected ? "尚未连接，指令已忽略" :
                    !status.handles_ready ? "键盘服务未就绪" :
                    !status.secure ? "等待配对完成" : "等待电脑启用键盘");
            break;
        }
        const kp_binding_t *binding = kp_binding(&s_model.config, s_model.triggered_slot);
        if (!kp_runner_start(&s_runner, &s_model.config, binding, event == KP_HOLD)) break;
        kp_set_busy(&s_model, true);
        uint32_t serial = s_model.feedback.serial + 1;
        s_model.feedback = (kp_feedback_t){.visible = true, .holding = s_runner.hold,
            .slot = s_model.triggered_slot, .total = binding->count,
            .state = KP_FEEDBACK_RUNNING, .serial = serial};
        message("正在执行");
        break;
    }
    case KP_RELEASE: kp_runner_release(&s_runner, now_ms()); break;
    case KP_CANCEL: if (s_runner.running) message("已取消"); kp_runner_cancel(&s_runner); release(); break;
    case KP_SELECT_PROFILE: s_target_profile = s_model.selected; s_select_profile = true; break;
    case KP_SAVE_SCREEN: s_save_screen = true; break;
    case KP_SAVE_SYSTEM: s_save_system = true; break;
    case KP_AP_START: kp_network_service_ap(true); break;
    case KP_AP_STOP: kp_network_service_ap(false); break;
    case KP_TRANSFER_START: kp_network_service_transfer(true); break;
    case KP_TRANSFER_STOP: kp_network_service_transfer(false); break;
    case KP_CLEAR_BOND: s_forget = true; break;
    }
}
static void advance_runner(void) {
    kp_action_t action;
    if (!kp_runner_request(&s_runner, now_ms(), &action)) return;
    kp_action_t resolved;
    esp_err_t sent = kp_action_resolve(&action, s_model.system, &resolved) ?
                     kp_ble_send(resolved.type == KP_NONE ? NULL : &resolved) : ESP_ERR_INVALID_ARG;
    uint32_t now = now_ms();
    kp_runner_result(&s_runner, sent == ESP_OK, now);
    s_model.feedback.completed = s_runner.completed;
    s_model.feedback.holding = s_runner.hold;
    if (!s_runner.running) {
        kp_set_busy(&s_model, false);
        s_model.feedback.state = s_runner.failed ? KP_FEEDBACK_FAILED : KP_FEEDBACK_DONE;
        s_model.feedback.until = now + KP_FEEDBACK_MS;
        if (s_runner.failed) {
            release();
            if (sent == ESP_ERR_NOT_SUPPORTED) message("电脑未启用媒体按键");
            else { kp_ble_recover(); message("发送失败，剩余步骤已取消"); }
        } else message("已完成");
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
static void sync_physical_input(void) {
    // Drop all queued events on mode/link transitions. Synchronize the physical
    // state as well, including a release that could have been lost on overflow.
    xQueueReset(s_queue);
    unsigned physical = atomic_load(&s_physical);
    if (physical != s_model.screen.down_mask) s_model.screen.last_activity = now_ms();
    s_model.down_mask = physical;
    s_model.screen.down_mask = s_model.down_mask;
    if (!physical) s_model.screen.waking = false;
}
static void resync_input(void) {
    sync_physical_input();
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
    kp_config_t *config = malloc(sizeof(*config));
    if (!config) { ESP_LOGE("keypad", "Configuration allocation failed"); return; }
    kp_store_get(config); kp_init(&s_model, config, effect, NULL); free(config);
    kp_store_screen_get(&s_model.screen_config);
    kp_store_system_get(&s_model.system);
    kp_screen_init(&s_model.screen, &s_model.screen_config, now_ms());
    s_model.page = KP_MENU;
    kp_home_config_t home; kp_store_home_get(&home);
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) { ESP_LOGE("keypad", "Display unavailable"); return; }
    uint8_t backlight = s_model.screen_config.brightness;
    bsp_display_backlight(backlight);
    bool battery_ok = bsp_battery_init() == ESP_OK;
    kp_ui_init();
    s_queue = xQueueCreate(24, sizeof(input_t));
    if (!s_queue) { ESP_LOGE("keypad", "Input queue allocation failed"); return; }
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    rc = kp_ble_init();
    if (rc != ESP_OK) message("蓝牙启动失败，请重启");
    ESP_ERROR_CHECK(kp_network_service_start());
    kp_ap_state_t previous_ap = KP_AP_OFF;
    memory_log("startup");
    uint32_t last_ui = 0, last_battery = 0, generation = 0;
    int battery = -1;
    for (;;) {
        uint32_t now = now_ms();
        kp_ble_status_t ble = kp_ble_status();
        if (ble.generation != generation) {
            bool interrupted = s_runner.running;
            generation = ble.generation; sync_physical_input(); kp_link_changed(&s_model, now);
            if (interrupted) message("连接状态变化，指令已停止");
        }
        if (atomic_exchange(&s_overflow, false)) { resync_input(); message("输入已恢复，请重新按键"); }
        input_t input;
        if (xQueueReceive(s_queue, &input, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Consume timestamped releases before advancing the hold timer to
            // wall time, so a delayed worker cannot invent a long press.
            do {
                if ((uint32_t)(now_ms() - input.at) > 250) { resync_input(); break; }
                kp_input(&s_model, input.key, input.down, input.at);
            } while (xQueueReceive(s_queue, &input, 0) == pdTRUE);
        }
        now = now_ms();
        kp_tick(&s_model, now);
        advance_runner();
        if (s_save_system) {
            s_save_system = false; resync_input();
            bool saved = kp_store_system_save(s_model.selected) == ESP_OK;
            kp_system_saved(&s_model, saved);
        }
        if (s_save_screen) {
            s_save_screen = false;
            bool saved = kp_store_screen_save(&s_model.screen_draft) == ESP_OK;
            kp_screen_saved(&s_model, saved);
            message(saved ? "屏幕设置已保存" : "保存失败，请重试");
        }
        if (s_select_profile) {
            s_select_profile = false; resync_input();
            uint8_t previous = s_model.config.active_profile;
            s_model.config.active_profile = s_target_profile;
            if (kp_store_save(&s_model.config) == ESP_OK) {
                ++s_model.config_revision; s_model.page = KP_CONSOLE; s_model.selected = 0;
                message("情景已切换");
            } else {
                s_model.config.active_profile = previous; message("保存失败，仍使用原情景");
            }
            resync_input();
        }
        kp_network_view_t network; kp_network_service_get(&network);
        s_model.transfer = network.transfer;
        if (network.ap != previous_ap) {
            if (network.ap == KP_AP_OFF && previous_ap != KP_AP_OFF) {
                s_ap_active = false; kp_store_get(&s_model.config); ++s_model.config_revision;
                kp_store_home_get(&home); message("热点已关闭");
                if (s_model.page == KP_HOTSPOT) { s_model.page = KP_SETTINGS; s_model.selected = 3; }
                if (network.forget_requested) { s_model.page = KP_FORGET; s_model.selected = 0; }
            } else if (network.ap == KP_AP_STARTING) message("正在开启热点");
            else if (network.ap == KP_AP_ACTIVE) { s_ap_active = true; message("配置完成后长按 OK 返回"); }
            else if (network.ap == KP_AP_STOPPING) message("正在关闭热点");
            else if (network.ap == KP_AP_FAILED) { s_model.page = KP_HOTSPOT; message("热点操作失败，长按重试"); }
            previous_ap = network.ap; resync_input();
        }
        if (s_forget) {
            s_forget = false; resync_input();
            message(kp_ble_forget() == ESP_OK ? "配对已清除，请重新连接" : "清除失败，请重试");
            resync_input();
        }
        uint8_t level = kp_screen_level(&s_model);
        if (level != backlight) { backlight = level; bsp_display_backlight(level); }
        if (battery_ok && (!last_battery || (uint32_t)(now - last_battery) >= 30000)) {
            battery = bsp_battery_soc(); last_battery = now;
        }
        if ((uint32_t)(now - last_ui) >= 100) {
            kp_ble_poll();
            kp_ui_update(&s_model, kp_ble_status(), battery, s_message,
                         s_ap_active ? network.ssid : "未开启", s_ap_active ? network.password : "--", &home,
                         kp_store_time_read((uint64_t)esp_timer_get_time() / 1000));
            last_ui = now;
        }
    }
}
