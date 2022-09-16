
#pragma     once


#include <stdint.h>
#include "events.h"

void tallyTrkObj_Initialize(void);
void tallyTrkObj_EventHandler(eventQueue_t* event);
int32_t tallyTrkObj_GetStickCount(void);

