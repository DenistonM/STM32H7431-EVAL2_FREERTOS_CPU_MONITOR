/* dashboard.c - draws the RTOS Monitor screen (640 x 480) with UTIL_LCD_* */
#include "dashboard.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_tasks.h"
#include <stdio.h>
#include <string.h>

/* ---- Colours (ARGB8888) ---- */
#define C_BG      0xFF07100CU
#define C_PANEL   0xFF0D1A14U
#define C_BORDER  0xFF1F3A2CU
#define C_TRACK   0xFF16291FU
#define C_TEXT    0xFFE6F4ECU
#define C_MUTED   0xFF8FB09FU
#define C_GREEN   0xFF3DDC84U
#define C_AMBER   0xFFF5B73BU
#define C_CYAN    0xFF4CC9F0U
#define C_RED     0xFFFF6B6BU
#define C_CHIP    0xFF14301FU
#define C_PADOFF  0xFF2A4A3AU

/* ---- Layout (pixels) ---- */
#define TASK_ROW_Y0   130U
#define TASK_ROW_H     22U
#define PAD_CX         52U
#define PAD_CY        338U
#define LOG_Y0        407U
#define LOG_PITCH      13U

/* ---- System log ring buffer ---- */
#define LOG_DEPTH      8U      /* entries kept            */
#define LOG_LINES      4U      /* lines shown on screen   */
#define LOG_MSG_LEN   48U

typedef struct
{
    uint32_t sec;
    char     msg[LOG_MSG_LEN];
} LogEntry_t;

static LogEntry_t       logBuf[LOG_DEPTH];
static volatile uint32_t logCount = 0;
static uint32_t         lastLogSeq = 0xFFFFFFFFU;

typedef struct
{
    const char *name;
    uint8_t     prio;        /* keep in sync with xTaskCreate() in app_tasks.c */
    uint16_t    stackWords;
} TaskRow_t;

static const TaskRow_t kRows[TASK_COUNT] =
{
    { "Input",   4, INPUT_STACK_WORDS    },
    { "Status",  3, STATUS_STACK_WORDS   },
    { "Button",  3, BUTTON_STACK_WORDS   },
    { "Display", 2, DISPLAY_STACK_WORDS  },
    { "Memory",  2, MEMORY_STACK_WORDS   },
    { "Work",    1, WORKLOAD_STACK_WORDS },
    { "LedMon",  1, LEDMON_STACK_WORDS   },
};

/* ---- Helpers ---- */
static void Txt(uint32_t x, uint32_t y, sFONT *f, uint32_t fg, uint32_t bg, const char *s)
{
    UTIL_LCD_SetFont(f);
    UTIL_LCD_SetTextColor(fg);
    UTIL_LCD_SetBackColor(bg);
    UTIL_LCD_DisplayStringAt(x, y, (uint8_t *)s, LEFT_MODE);
}

static void Card(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    UTIL_LCD_FillRect(x, y, w, h, C_PANEL);
    UTIL_LCD_DrawRect(x, y, w, h, C_BORDER);
}

/* Fill first, then track: no flicker because nothing is cleared first */
static void Bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pct, uint32_t color)
{
    uint32_t fw;
    if (pct > 100U) { pct = 100U; }
    fw = (w * pct) / 100U;
    if (fw > 0U)  { UTIL_LCD_FillRect(x, y, fw, h, color); }
    if (fw < w)   { UTIL_LCD_FillRect(x + fw, y, w - fw, h, C_TRACK); }
}

/* ---- System log ---- */
void Dashboard_Log(const char *msg)
{
    uint32_t sec = (uint32_t)xTaskGetTickCount() / (uint32_t)configTICK_RATE_HZ;

    taskENTER_CRITICAL();
    {
        LogEntry_t *e = &logBuf[logCount % LOG_DEPTH];
        e->sec = sec;
        strncpy(e->msg, msg, LOG_MSG_LEN - 1U);
        e->msg[LOG_MSG_LEN - 1U] = '\0';
        logCount++;
    }
    taskEXIT_CRITICAL();
}

static void DrawLog(void)
{
    LogEntry_t e[LOG_LINES];
    char t[16];
    char m[64];
    uint32_t seq, n, i;

    taskENTER_CRITICAL();
    seq = logCount;
    n = (seq < LOG_LINES) ? seq : LOG_LINES;
    for (i = 0; i < n; i++)
    {
        e[i] = logBuf[(seq - n + i) % LOG_DEPTH];
    }
    taskEXIT_CRITICAL();

    if (seq == lastLogSeq) { return; }      /* nothing new: skip redraw */
    lastLogSeq = seq;

    for (i = 0; i < LOG_LINES; i++)
    {
        uint32_t y = LOG_Y0 + (i * LOG_PITCH);
        if (i < n)
        {
            snprintf(t, sizeof t, "[%02lu:%02lu:%02lu]", (unsigned long)(e[i].sec / 3600U),
                     (unsigned long)((e[i].sec / 60U) % 60U), (unsigned long)(e[i].sec % 60U));
            Txt(22, y, &Font12, C_GREEN, C_PANEL, t);
            snprintf(m, sizeof m, "%-60s", e[i].msg);
            Txt(100, y, &Font12, (i == (n - 1U)) ? C_TEXT : C_MUTED, C_PANEL, m);
        }
        else
        {
            UTIL_LCD_FillRect(16, y, 600, 12, C_PANEL);
        }
    }
}

/* ---- Static part ---- */
void Dashboard_DrawStatic(void)
{
    char s[40];
    uint32_t i;

    UTIL_LCD_FillRect(0, 0, 640, 480, C_BG);

    /* Header */
    snprintf(s, sizeof s, "STM32H743I-EVAL  %lu MHz", (unsigned long)(SystemCoreClock / 1000000UL));
    Txt(12, 24, &Font12, C_MUTED, C_BG, s);
    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(C_AMBER);
    UTIL_LCD_SetBackColor(C_BG);
    UTIL_LCD_DisplayStringAt(0, 14, (uint8_t *)"RTOS MONITOR", CENTER_MODE);
    UTIL_LCD_FillRect(558, 14, 70, 22, C_CHIP);
    UTIL_LCD_DrawRect(558, 14, 70, 22, 0xFF2F7A4DU);
    Txt(568, 20, &Font12, C_GREEN, C_CHIP, "RUNNING");

    /* Row 1: Uptime / Ticks / CPU load */
    Card(12, 52, 216, 48);
    Card(236, 52, 216, 48);
    Card(460, 52, 168, 48);
    Txt(22, 58, &Font12, C_MUTED, C_PANEL, "UPTIME");
    snprintf(s, sizeof s, "TICKS  %lu Hz", (unsigned long)configTICK_RATE_HZ);
    Txt(246, 58, &Font12, C_MUTED, C_PANEL, s);
    Txt(470, 58, &Font12, C_MUTED, C_PANEL, "CPU LOAD");

    /* Row 2: Tasks card */
    Card(12, 108, 412, 180);
    Txt(22, 113, &Font12, C_MUTED, C_PANEL, "TASKS");
    Txt(98, 113, &Font12, C_MUTED, C_PANEL, "PRI");
    Txt(130, 113, &Font12, C_MUTED, C_PANEL, "STATE");
    Txt(190, 113, &Font12, C_MUTED, C_PANEL, "STACK USED");
    for (i = 0; i < TASK_COUNT; i++)
    {
        uint32_t y = TASK_ROW_Y0 + (i * TASK_ROW_H);
        UTIL_LCD_DrawHLine(14, y - 2, 408, C_TRACK);
        Txt(22, y + 3, &Font16, C_TEXT, C_PANEL, kRows[i].name);
        UTIL_LCD_FillRect(98, y + 2, 22, 18, C_CHIP);
        snprintf(s, sizeof s, "%u", (unsigned)kRows[i].prio);
        Txt(104, y + 3, &Font16, C_GREEN, C_CHIP, s);
    }

    /* Row 2: Heap + Workload */
    Card(432, 108, 196, 112);
    snprintf(s, sizeof s, "HEAP  %lu B", (unsigned long)configTOTAL_HEAP_SIZE);
    Txt(442, 114, &Font12, C_MUTED, C_PANEL, s);
    Card(432, 228, 196, 60);
    Txt(442, 234, &Font12, C_MUTED, C_PANEL, "WORKLOAD CYCLES");

    /* Row 3: Input card + ADC card + LED card */
    Card(12, 296, 290, 84);
    UTIL_LCD_DrawCircle(PAD_CX, PAD_CY, 32, C_BORDER);
    Txt(100, 306, &Font12, C_MUTED, C_PANEL, "JOY");
    Txt(100, 328, &Font12, C_MUTED, C_PANEL, "PRESS");
    Txt(100, 350, &Font12, C_MUTED, C_PANEL, "LAST");
    Txt(226, 304, &Font12, C_MUTED, C_PANEL, "TAMPER");
    Txt(226, 340, &Font12, C_MUTED, C_PANEL, "WAKEUP");

    Card(310, 296, 150, 84);
    Txt(320, 302, &Font12, C_MUTED, C_PANEL, "ADC  PA0_C");

    Card(468, 296, 160, 84);
    Txt(478, 302, &Font12, C_MUTED, C_PANEL, "LEDS");
    Txt(496, 320, &Font12, C_TEXT, C_PANEL, "Heartbeat");
    Txt(496, 340, &Font12, C_TEXT, C_PANEL, "Workload");
    Txt(496, 360, &Font12, C_TEXT, C_PANEL, "Joystick");

    /* Row 4: System log */
    Card(12, 388, 616, 74);
    Txt(22, 393, &Font12, C_MUTED, C_PANEL, "SYSTEM LOG");
    lastLogSeq = 0xFFFFFFFFU;               /* force the log to be redrawn */

    /* Footer */
    Txt(474, 468, &Font12, C_MUTED, C_BG, "Display refresh 200 ms");
}

/* ---- Dynamic part ---- */
static const char *StateName(eTaskState st, uint32_t *color)
{
    switch (st)
    {
        case eRunning:   *color = C_GREEN; return "RUN";
        case eReady:     *color = C_CYAN;  return "RDY";
        case eBlocked:   *color = C_MUTED; return "BLK";
        case eSuspended: *color = C_RED;   return "SUS";
        default:         *color = C_RED;   return "---";
    }
}

static void DrawPad(uint32_t joy)
{
    const uint32_t cx = PAD_CX, cy = PAD_CY;
    UTIL_LCD_FillRect(cx - 5,  cy - 27, 10, 12, (joy & JOY_UP)    ? C_GREEN : C_PADOFF);
    UTIL_LCD_FillRect(cx - 5,  cy + 15, 10, 12, (joy & JOY_DOWN)  ? C_GREEN : C_PADOFF);
    UTIL_LCD_FillRect(cx - 27, cy - 5,  12, 10, (joy & JOY_LEFT)  ? C_GREEN : C_PADOFF);
    UTIL_LCD_FillRect(cx + 15, cy - 5,  12, 10, (joy & JOY_RIGHT) ? C_GREEN : C_PADOFF);
    UTIL_LCD_FillCircle(cx, cy, 9, (joy & JOY_SEL) ? C_GREEN : C_PADOFF);
}

static void DrawLed(uint32_t y, uint32_t on, uint32_t color)
{
    UTIL_LCD_FillCircle(484, y + 6, 5, on ? color : C_BORDER);
}

void Dashboard_Update(uint32_t pressCount, uint32_t lastEvent)
{
    char s[32];
    uint32_t i;
    uint32_t ticks = (uint32_t)xTaskGetTickCount();
    uint32_t sec   = ticks / (uint32_t)configTICK_RATE_HZ;
    uint32_t total = (uint32_t)configTOTAL_HEAP_SIZE;
    uint32_t freeH = gStats.freeHeap;
    uint32_t usedH, usedPct, cpu;

    /* Row 1 */
    snprintf(s, sizeof s, "%02lu:%02lu:%02lu ", (unsigned long)(sec / 3600U),
             (unsigned long)((sec / 60U) % 60U), (unsigned long)(sec % 60U));
    Txt(22, 74, &Font24, C_TEXT, C_PANEL, s);
    snprintf(s, sizeof s, "%-8lu", (unsigned long)ticks);
    Txt(246, 74, &Font24, C_TEXT, C_PANEL, s);

    cpu = gStats.cpuLoad;
    if (cpu > 100U) { cpu = 100U; }
    snprintf(s, sizeof s, "%3lu%%", (unsigned long)cpu);
    Txt(470, 74, &Font24, C_TEXT, C_PANEL, s);
    Bar(546, 82, 72, 10, cpu, (cpu > 90U) ? C_RED : ((cpu > 70U) ? C_AMBER : C_GREEN));

    /* Tasks */
    for (i = 0; i < TASK_COUNT; i++)
    {
        uint32_t y = TASK_ROW_Y0 + (i * TASK_ROW_H);
        uint32_t color = C_RED;
        uint32_t pct = 0;
        const char *name = "---";
        TaskHandle_t h = gTaskHandle[i];

        if (h != NULL)
        {
            uint32_t freeW = (uint32_t)uxTaskGetStackHighWaterMark(h);
            uint32_t sz = kRows[i].stackWords;
            pct = (freeW >= sz) ? 0U : (((sz - freeW) * 100U) / sz);
            name = StateName(eTaskGetState(h), &color);
        }
        snprintf(s, sizeof s, "%-4s", name);
        Txt(130, y + 3, &Font16, color, C_PANEL, s);
        Bar(190, y + 7, 150, 8, pct, (pct > 80U) ? C_AMBER : C_GREEN);
        snprintf(s, sizeof s, "%3lu%%", (unsigned long)pct);
        Txt(350, y + 5, &Font12, C_MUTED, C_PANEL, s);
    }

    /* Heap */
    if (freeH > total) { freeH = total; }
    usedH   = total - freeH;
    usedPct = (usedH * 100U) / total;
    snprintf(s, sizeof s, "%-6lu", (unsigned long)freeH);
    Txt(442, 130, &Font24, C_GREEN, C_PANEL, s);
    Txt(552, 136, &Font12, C_MUTED, C_PANEL, "free");
    Bar(442, 162, 176, 8, usedPct, C_AMBER);
    snprintf(s, sizeof s, "min free %-6lu", (unsigned long)gStats.minFreeHeap);
    Txt(442, 176, &Font12, C_MUTED, C_PANEL, s);
    snprintf(s, sizeof s, "used     %-6lu", (unsigned long)usedH);
    Txt(442, 192, &Font12, C_TEXT, C_PANEL, s);

    /* Workload */
    snprintf(s, sizeof s, "%-8lu", (unsigned long)gStats.workloadCount);
    Txt(442, 250, &Font24, C_TEXT, C_PANEL, s);

    /* Input card */
    DrawPad(gStats.joystick);
    snprintf(s, sizeof s, "%-6lu", (unsigned long)gStats.joystick);
    Txt(148, 304, &Font16, C_TEXT, C_PANEL, s);
    snprintf(s, sizeof s, "%-6lu", (unsigned long)pressCount);
    Txt(148, 326, &Font16, C_TEXT, C_PANEL, s);
    snprintf(s, sizeof s, "%-6lu", (unsigned long)lastEvent);
    Txt(148, 348, &Font16, C_TEXT, C_PANEL, s);
    snprintf(s, sizeof s, "%-4lu", (unsigned long)gStats.tamperCount);
    Txt(226, 318, &Font16, C_AMBER, C_PANEL, s);
    snprintf(s, sizeof s, "%-4lu", (unsigned long)gStats.wakeCount);
    Txt(226, 354, &Font16, C_AMBER, C_PANEL, s);
    UTIL_LCD_FillCircle(288, 362, 5, gStats.wakePressed ? C_GREEN : C_BORDER);

    /* ADC card: 12-bit value, assuming VREF+ = 3.3 V */
    if (gStats.adcOk != 0U)
    {
        uint32_t raw = gStats.adcRaw;
        uint32_t mv, apct;
        if (raw > 4095U) { raw = 4095U; }
        mv   = (raw * 3300U) / 4095U;
        apct = (raw * 100U) / 4095U;
        snprintf(s, sizeof s, "%lu.%02lu V ", (unsigned long)(mv / 1000U), (unsigned long)((mv % 1000U) / 10U));
        Txt(320, 318, &Font24, C_TEXT, C_PANEL, s);
        Bar(320, 348, 130, 8, apct, C_CYAN);
        snprintf(s, sizeof s, "raw %-4lu %3lu%%", (unsigned long)raw, (unsigned long)apct);
        Txt(320, 362, &Font12, C_MUTED, C_PANEL, s);
    }
    else
    {
        Txt(320, 318, &Font24, C_RED, C_PANEL, "n/a   ");
    }

    /* LEDs */
    DrawLed(320, gStats.ledGreen,  C_GREEN);
    DrawLed(340, gStats.ledOrange, C_AMBER);
    DrawLed(360, gStats.ledBlue,   C_CYAN);

    /* System log (redraws only when a new line was added) */
    DrawLog();
}

void Dashboard_DrawStatusTick(uint32_t n)
{
    char s[32];
    snprintf(s, sizeof s, "Status tick: %-8lu", (unsigned long)n);
    Txt(12, 468, &Font12, C_GREEN, C_BG, s);
}
