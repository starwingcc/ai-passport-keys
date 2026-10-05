#include "keypad_home.h"
#include "keypad_model.h"
#include <stdio.h>
#include <string.h>

void kp_home_defaults(kp_home_config_t *h) {
    memset(h, 0, sizeof(*h)); h->version = 1;
    memcpy(h->name, "USER", 5); h->timezone_minutes = 480;
}
bool kp_home_valid(const kp_home_config_t *h, bool stored) {
    if (!h || h->version != 1 || h->avatar > KP_AVATAR_CHIP || h->hour12 > 1 ||
        h->timezone_minutes < -720 || h->timezone_minutes > 840 || h->timezone_minutes % 15 ||
        !memchr(h->name, 0, sizeof(h->name)) || !h->name[0] ||
        !memchr(h->badge, 0, sizeof(h->badge))) return false;
    return stored ? kp_text_utf8_valid(h->name, KP_HOME_CHARS, false) && kp_text_utf8_valid(h->badge, KP_HOME_CHARS, false) :
                    kp_text_valid(h->name, KP_HOME_CHARS, false) && kp_text_valid(h->badge, KP_HOME_CHARS, false);
}
bool kp_clock_sync(kp_clock_t *c, uint64_t epoch, uint64_t now) {
    if (!c || epoch < KP_TIME_MIN || epoch > KP_TIME_MAX) return false;
    *c = (kp_clock_t){epoch, now}; return true;
}
uint64_t kp_clock_read(const kp_clock_t *c, uint64_t now) {
    if (!c || !c->epoch || now < c->monotonic_ms) return 0;
    uint64_t elapsed = (now - c->monotonic_ms) / 1000;
    return elapsed <= KP_TIME_MAX - c->epoch ? c->epoch + elapsed : 0;
}
static bool leap(unsigned y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
void kp_home_clock_text(const kp_home_config_t *h, uint64_t epoch, char time[6], char date[15], char period[3]) {
    strcpy(time, "--:--"); strcpy(date, "----/--/-- ---"); period[0] = 0;
    if (epoch < KP_TIME_MIN || epoch > KP_TIME_MAX) return;
    int64_t local = (int64_t)epoch + (int64_t)h->timezone_minutes * 60;
    unsigned days = (unsigned)(local / 86400), day = days, year = 1970, month = 0;
    while (day >= (leap(year) ? 366U : 365U)) { day -= leap(year) ? 366 : 365; ++year; }
    static const unsigned months[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    while (month < 11) {
        unsigned count = months[month] + (month == 1 && leap(year));
        if (day < count) break;
        day -= count; ++month;
    }
    unsigned hour = (unsigned)(local % 86400) / 3600, minute = (unsigned)(local % 3600) / 60;
    if (h->hour12) { strcpy(period, hour < 12 ? "AM" : "PM"); hour = hour % 12 ? hour % 12 : 12; }
    snprintf(time, 6, "%02u:%02u", hour, minute);
    static const char *week[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    snprintf(date, 15, "%04u/%02u/%02u %s", year, month + 1, day + 1, week[(days + 4) % 7]);
}
