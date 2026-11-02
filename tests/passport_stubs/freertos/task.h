#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreate(TaskFunction_t,const char *,uint32_t,void *,UBaseType_t,TaskHandle_t *);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskDelete(TaskHandle_t);
void vTaskSuspend(TaskHandle_t);
void vTaskDelay(TickType_t);
