#include "keypad_pairing.h"
#include <string.h>

enum { RETRY_LIMIT = 4, RETRY_WINDOW_MS = 10000 };

void kp_pairing_reset(kp_pairing_t *state) {
    memset(state, 0, sizeof(*state));
}
void kp_pairing_missing_key(kp_pairing_t *state) {
    state->hint = KP_PAIRING_KEY_MISSING;
}
void kp_pairing_disconnected(kp_pairing_t *state, uint32_t now,
                            const uint8_t peer[7], bool candidate) {
    if (!candidate) {
        state->failures = 0;
        return;
    }
    if (!state->failures || (uint32_t)(now - state->window_start) > RETRY_WINDOW_MS ||
        memcmp(state->peer, peer, sizeof(state->peer))) {
        state->window_start = now;
        state->failures = 0;
        memcpy(state->peer, peer, sizeof(state->peer));
    }
    if (state->failures < RETRY_LIMIT) ++state->failures;
    if (state->failures == RETRY_LIMIT && state->hint == KP_PAIRING_NONE)
        state->hint = KP_PAIRING_RETRY_FAILED;
}
const char *kp_pairing_title(kp_pairing_hint_t hint) {
    return hint == KP_PAIRING_KEY_MISSING ? "配对记录已失效" : "配对连接异常";
}
const char *kp_pairing_instructions(kp_pairing_hint_t hint) {
    return hint == KP_PAIRING_KEY_MISSING ?
        "配对记录不一致。\n请在电脑蓝牙设置中\n删除 / 忽略旧设备\nPassport Keys，\n然后重新配对。" :
        "可能保留了旧配对。\n请尝试在电脑蓝牙设置中\n删除 / 忽略旧设备\nPassport Keys，\n然后重新配对。";
}
