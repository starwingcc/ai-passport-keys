#include "keypad_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    kp_screen_config_t c; kp_screen_defaults(&c);
    assert(kp_screen_valid(&c) && c.brightness == 80 && c.dim_brightness == 20);
    kp_screen_config_t bad = c;
    bad.off_after_s = 10; assert(!kp_screen_valid(&bad));
    bad = c; bad.dim_after_s = 30; assert(!kp_screen_valid(&bad));
    bad = c; bad.dim_brightness = 85; assert(!kp_screen_valid(&bad));
    bad = c; bad.reserved[0] = 1; assert(!kp_screen_valid(&bad));
    bad = c; bad.version = 2; assert(!kp_screen_valid(&bad));
    bad = c; bad.brightness = 0; assert(!kp_screen_valid(&bad));
    bad = c; bad.dim_brightness = 7; assert(!kp_screen_valid(&bad));
    bad = c; bad.off_after_s = 25; assert(!kp_screen_valid(&bad));
    bad = c; bad.dim_after_s = 15; assert(!kp_screen_valid(&bad));
    bad = c; bad.off_after_s = 310; assert(!kp_screen_valid(&bad));
    bad = c; bad.off_after_s = 300; bad.dim_after_s = 190; assert(!kp_screen_valid(&bad));
    assert(!kp_screen_valid(NULL));
    // Every reachable edit remains valid, including threshold filtering.
    for (unsigned setting = 0; setting < KP_SCREEN_SETTING_COUNT; ++setting)
        for (int direction = -1; direction <= 1; direction += 2)
            for (unsigned n = 0; n < 100; ++n) {
                kp_screen_adjust(&c, setting, direction); assert(kp_screen_valid(&c));
            }
    kp_screen_defaults(&c);
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && c.dim_after_s == 20);
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && !c.dim_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && c.dim_after_s == 10);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, -1) && c.off_after_s == 20);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, -1) && !c.off_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, 1) && c.off_after_s == 20);
    c.off_after_s = 70; c.dim_after_s = 60;
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, -1) && !c.off_after_s);
    // Every 10-second value is reachable with the other stage disabled.
    kp_screen_defaults(&c); c.off_after_s = 0; c.dim_after_s = 0;
    for (unsigned n = 1; n <= 18; ++n) {
        assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && c.dim_after_s == n * 10);
    }
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && !c.dim_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, -1) && c.dim_after_s == 180);
    c.dim_after_s = 0;
    for (unsigned n = 1; n <= 30; ++n) {
        assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, 1) && c.off_after_s == n * 10);
    }
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, 1) && !c.off_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, -1) && c.off_after_s == 300);
    c.dim_after_s = 180; c.off_after_s = 190;
    assert(kp_screen_valid(&c));
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, 1) && !c.dim_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_AFTER, -1) && c.dim_after_s == 180);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, -1) && !c.off_after_s);
    assert(kp_screen_adjust(&c, KP_SCREEN_OFF_AFTER, 1) && c.off_after_s == 190);
    kp_screen_defaults(&c);
    for (unsigned i = 0; i < 14; ++i) kp_screen_adjust(&c, KP_SCREEN_BRIGHTNESS, -1);
    assert(c.brightness == 10 && c.dim_brightness == 10);
    assert(!kp_screen_adjust(&c, KP_SCREEN_BRIGHTNESS, -1));
    assert(kp_screen_adjust(&c, KP_SCREEN_DIM_BRIGHTNESS, -1) && c.dim_brightness == 5);
    assert(!kp_screen_adjust(&c, KP_SCREEN_DIM_BRIGHTNESS, -1));
    // Both stages are absolute deadlines from the same last operation.
    kp_screen_defaults(&c); kp_screen_t s; kp_screen_init(&s, &c, 100);
    kp_screen_tick(&s, &c, 10099, false); assert(s.state == KP_SCREEN_ACTIVE && s.level == 80);
    kp_screen_tick(&s, &c, 10100, false); assert(s.state == KP_SCREEN_DIM && s.level == 80);
    kp_screen_tick(&s, &c, 10250, false); assert(s.level == 50);
    kp_screen_tick(&s, &c, 10400, false); assert(s.level == 20);
    kp_screen_tick(&s, &c, 30099, false); assert(s.state == KP_SCREEN_DIM);
    kp_screen_tick(&s, &c, 30100, false); assert(s.state == KP_SCREEN_OFF && !s.level);
    // All edges until the wake gesture is fully released are consumed.
    assert(!kp_screen_input(&s, 2, true, 30200));
    kp_screen_tick(&s, &c, 30800, false); assert(s.state == KP_SCREEN_ACTIVE && s.level == 80);
    assert(!kp_screen_input(&s, 0, true, 30810));
    assert(!kp_screen_input(&s, 2, false, 30820));
    assert(!kp_screen_input(&s, 0, false, 30830) && !s.waking);
    assert(kp_screen_input(&s, 2, true, 30900));
    kp_screen_tick(&s, &c, 90000, false); assert(s.state == KP_SCREEN_ACTIVE);
    assert(kp_screen_input(&s, 2, false, 90010));
    kp_screen_tick(&s, &c, 100009, false); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 100010, false); assert(s.state == KP_SCREEN_DIM);
    assert(kp_screen_input(&s, 1, true, 100100));
    kp_screen_tick(&s, &c, 100100, false); assert(s.level == 80); // Interrupts a fade immediately.
    kp_screen_input(&s, 1, false, 100110);
    // Editing and running actions suspend timers; timing restarts on leaving.
    kp_screen_tick(&s, &c, 200000, true); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 209999, false); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 210000, false); assert(s.state == KP_SCREEN_DIM);
    // Either stage can be disabled without disabling the other.
    c.dim_after_s = 0; kp_screen_init(&s, &c, 0);
    kp_screen_tick(&s, &c, 29999, false); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 30000, false); assert(s.state == KP_SCREEN_OFF);
    c.off_after_s = 0; c.dim_after_s = 10; kp_screen_init(&s, &c, 0);
    kp_screen_tick(&s, &c, 10000, false); kp_screen_tick(&s, &c, 300000, false);
    assert(s.state == KP_SCREEN_DIM && s.level == 20);
    c.dim_after_s = 0; kp_screen_tick(&s, &c, 400000, false); assert(s.level == 80);
    // The new longest dim delay and a single-step gap use absolute deadlines.
    c.dim_after_s = 180; c.off_after_s = 190; kp_screen_init(&s, &c, 0);
    kp_screen_tick(&s, &c, 179999, false); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 180000, false); assert(s.state == KP_SCREEN_DIM);
    kp_screen_tick(&s, &c, 180300, false); assert(s.level == 20);
    kp_screen_tick(&s, &c, 189999, false); assert(s.state == KP_SCREEN_DIM);
    kp_screen_tick(&s, &c, 190000, false); assert(s.state == KP_SCREEN_OFF && !s.level);
    c.dim_after_s = 0; c.off_after_s = 10; kp_screen_init(&s, &c, 0);
    kp_screen_tick(&s, &c, 9999, false); assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, 10000, false); assert(s.state == KP_SCREEN_OFF);
    // Idle deadlines and the dim fade survive the millisecond counter wrapping.
    kp_screen_defaults(&c); uint32_t start = UINT32_MAX - 5000;
    kp_screen_init(&s, &c, start); kp_screen_tick(&s, &c, start + 9999, false);
    assert(s.state == KP_SCREEN_ACTIVE);
    kp_screen_tick(&s, &c, start + 10000, false); assert(s.state == KP_SCREEN_DIM);
    kp_screen_tick(&s, &c, start + 10300, false); assert(s.level == 20);
    kp_screen_tick(&s, &c, start + 30000, false); assert(s.state == KP_SCREEN_OFF);
    c.dim_after_s = 0; kp_screen_tick(&s, &c, start + 30001, false); assert(!s.level);
    kp_screen_defaults(&c); kp_screen_init(&s, &c, UINT32_MAX - 10100);
    kp_screen_tick(&s, &c, UINT32_MAX - 100, false); kp_screen_tick(&s, &c, 199, false);
    assert(s.state == KP_SCREEN_DIM && s.level == 20);
    puts("Screen settings, absolute idle deadlines, fade, wake filtering and timer wrap: PASS");
}
