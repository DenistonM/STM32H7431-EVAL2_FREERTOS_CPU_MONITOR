#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* Stack sizes (in words) - shared with dashboard.c to compute stack usage % */
#define DISPLAY_STACK_WORDS  640
#define INPUT_STACK_WORDS    384
#define MEMORY_STACK_WORDS   384
#define WORKLOAD_STACK_WORDS 256
#define LEDMON_STACK_WORDS   256
#define STATUS_STACK_WORDS   384
#define BUTTON_STACK_WORDS   256

/* Order = rows of the task table on screen */
typedef enum
{
    TASK_INPUT = 0,
    TASK_STATUS,
    TASK_BUTTON,
    TASK_DISPLAY,
    TASK_MEMORY,
    TASK_WORK,
    TASK_LEDMON,
    TASK_COUNT
} TaskId_t;

typedef struct
{
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    uint32_t workloadCount;
    uint32_t joystick;
    uint32_t tamperCount;
    uint32_t ledGreen;     /* 1 = on, tracked by LedMonitorTask */
    uint32_t ledOrange;
    uint32_t ledBlue;
    uint32_t cpuLoad;      /* percent, 0..100, measured by MemoryTask */
    uint32_t adcOk;        /* 1 = ADC initialised */
    uint32_t adcRaw;       /* 12-bit potentiometer value 0..4095 (filtered) */
    uint32_t wakeCount;    /* Wakeup button presses */
    uint32_t wakePressed;  /* 1 while the Wakeup button is held */
} SystemStats_t;

extern volatile SystemStats_t gStats;
extern TaskHandle_t gTaskHandle[TASK_COUNT];

void App_CreateTasks(void);

/* Run-time-stats timer (DWT cycle counter) used by FreeRTOS for CPU load */
void     App_RunTimeTimerInit(void);
uint32_t App_RunTimeCounter(void);

#endif
