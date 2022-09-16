

#pragma once

#include <stdint.h>
#include "events.h"

void updateTimerEvents(void);
void timerObj_Initialize(void);
void timerObj_EventHandler(eventQueue_t *event);

uint32_t  getTime_mSec(void);
uint32_t getTimeDiff_mSec(uint32_t startTime, uint32_t endTime);
void showAllTimers(void);

