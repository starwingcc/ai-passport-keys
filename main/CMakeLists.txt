idf_component_register(
    SRCS "main.c"
         "keyboard/keypad_model.c" "keyboard/keypad_text.c" "keyboard/keypad_runner.c" "keyboard/keypad_config.c"
         "keyboard/keypad_store.c" "keyboard/keypad_ble.c"
         "keyboard/keypad_web.c" "keyboard/keypad_ui.c"
         "../assets/fonts/passport_font_16.c" "../assets/fonts/passport_font_20.c" "../assets/fonts/passport_font_28.c"
    INCLUDE_DIRS "." "keyboard"
    REQUIRES bsp bt esp_hid esp_event esp_netif esp_timer esp_wifi nvs_flash
             esp_http_server json esp_hw_support
    EMBED_TXTFILES "web/index.html" "web/app.js" "web/app.css" "web/charset.js"
)
