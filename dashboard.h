#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdint.h>

/* Draw the fixed parts of the screen (frames, labels, task names). Call once. */
void Dashboard_DrawStatic(void);

/* Refresh every changing value. Call periodically (e.g. every 200 ms). */
void Dashboard_Update(uint32_t pressCount, uint32_t lastEvent);

/* Small counter at the bottom-left of the screen. */
void Dashboard_DrawStatusTick(uint32_t n);

/* Add one line to the SYSTEM LOG (safe to call from any task, not from an ISR).
   Keep the text short (max 47 characters). */
void Dashboard_Log(const char *msg);

#endif
