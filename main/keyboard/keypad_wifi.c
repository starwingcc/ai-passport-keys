#include "keypad_wifi.h"
#include "keypad_model.h"
#include <string.h>

void kp_wifi_defaults(kp_wifi_config_t *wifi) {
    *wifi = (kp_wifi_config_t){.version = 1};
}
bool kp_wifi_valid(const kp_wifi_config_t *wifi) {
    if (!wifi || wifi->version != 1 ||
        !memchr(wifi->ssid, 0, sizeof(wifi->ssid)) ||
        !memchr(wifi->password, 0, sizeof(wifi->password)) ||
        !kp_text_utf8_valid(wifi->ssid, 32, false)) return false;
    size_t length = strlen(wifi->password);
    if (!wifi->ssid[0]) return length == 0;
    if (length && length < 8) return false;
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char)wifi->password[i] < 32 || (unsigned char)wifi->password[i] > 126) return false;
    return true;
}
