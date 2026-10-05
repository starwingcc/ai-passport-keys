#pragma once
#include "keypad_home.h"
#include "keypad_ble.h"
#include "lvgl.h"
// Call only from the LVGL task or under bsp_lvgl_lock().
void kp_home_ui_init(lv_obj_t *screen);
void kp_home_ui_set(bool show, const kp_home_config_t *home, kp_ble_status_t ble,
                    int battery, unsigned menu, uint64_t epoch);
