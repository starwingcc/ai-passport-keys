cmake_minimum_required(VERSION 3.16)
project(keypad_preview C CXX)
get_filename_component(ROOT "../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(keypad_preview preview.c "${ROOT}/main/keyboard/keypad_ui.c"
    "${ROOT}/main/keyboard/keypad_home.c" "${ROOT}/main/keyboard/keypad_home_ui.c"
    "${ROOT}/main/keyboard/keypad_model.c" "${ROOT}/main/keyboard/keypad_devices.c" "${ROOT}/main/keyboard/keypad_text.c" "${ROOT}/main/keyboard/keypad_screen.c"
    "${ROOT}/main/keyboard/keypad_pairing.c"
    "${ROOT}/assets/fonts/passport_font_16.c" "${ROOT}/assets/fonts/passport_font_20.c" "${ROOT}/assets/fonts/passport_font_28.c"
    "${ROOT}/assets/fonts/passport_font_20_menu.c")
target_include_directories(keypad_preview PRIVATE stubs "${ROOT}/main/keyboard")
target_link_libraries(keypad_preview PRIVATE lvgl m)
