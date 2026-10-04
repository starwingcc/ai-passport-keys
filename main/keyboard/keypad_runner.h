#pragma once
#include "keypad_model.h"
typedef enum { KP_RUN_READY, KP_RUN_PRESSED, KP_RUN_WAIT, KP_RUN_HOLD } kp_run_phase_t;
typedef struct {
    const kp_config_t *config;
    kp_binding_t binding;
    bool running, pending, hold, failed;
    uint8_t index, completed;
    kp_run_phase_t phase;
    uint32_t deadline;
} kp_runner_t;
bool kp_runner_start(kp_runner_t *runner, const kp_config_t *config, const kp_binding_t *binding, bool hold);
// Request one report; acknowledge it using the actual completion timestamp.
bool kp_runner_request(kp_runner_t *runner, uint32_t now, kp_action_t *action);
void kp_runner_result(kp_runner_t *runner, bool success, uint32_t now);
void kp_runner_release(kp_runner_t *runner, uint32_t now);
void kp_runner_cancel(kp_runner_t *runner);
