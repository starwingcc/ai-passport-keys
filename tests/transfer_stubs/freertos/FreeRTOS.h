#pragma once
#include <stdbool.h>
#include <stdint.h>
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
typedef unsigned TickType_t;
#define pdPASS 1
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
