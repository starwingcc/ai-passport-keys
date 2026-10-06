#pragma once
#include "FreeRTOS.h"
void vTaskDelay(TickType_t);
typedef void *TaskHandle_t;
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
