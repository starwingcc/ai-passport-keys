#include "keypad_model.h"
#include "keypad_charset.h"
#include <string.h>

bool kp_codepoint_supported(uint32_t cp) {
    size_t lo = 0, hi = sizeof(kp_character_ranges) / sizeof(kp_character_ranges[0]);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cp < kp_character_ranges[mid][0]) hi = mid;
        else if (cp > kp_character_ranges[mid][1]) lo = mid + 1;
        else return true;
    }
    return false;
}
static bool text_valid(const char *text, unsigned limit, bool multiline, bool display_coverage) {
    if (!text) return false;
    const unsigned char *p = (const unsigned char *)text;
    unsigned count = 0;
    while (*p) {
        uint32_t cp; unsigned extra; uint32_t minimum;
        if (*p < 0x80) { cp = *p++; extra = 0; minimum = 0; }
        else if (*p >= 0xc2 && *p <= 0xdf) { cp = *p++ & 0x1f; extra = 1; minimum = 0x80; }
        else if (*p >= 0xe0 && *p <= 0xef) { cp = *p++ & 0x0f; extra = 2; minimum = 0x800; }
        else if (*p >= 0xf0 && *p <= 0xf4) { cp = *p++ & 7; extra = 3; minimum = 0x10000; }
        else return false;
        for (unsigned i = 0; i < extra; ++i) {
            if ((*p & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (*p++ & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        if (++count > limit) return false;
        if (multiline && cp == '\n') continue;
        if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f) ||
            (display_coverage && !kp_codepoint_supported(cp))) return false;
    }
    return true;
}
bool kp_text_valid(const char *text, unsigned limit, bool multiline) {
    return text_valid(text, limit, multiline, true);
}
bool kp_text_utf8_valid(const char *text, unsigned limit, bool multiline) {
    return text_valid(text, limit, multiline, false);
}
