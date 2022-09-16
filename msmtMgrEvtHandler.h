

#pragma     once


#include "events.h"
#include "calibration.h"

#define FORCED_HEIGHT_VALUE       220
//  #define FORCED_WEIGHT_VALUE      250


void msmtMgrObj_Initialize(void);
void msmtMgrObj_EventHandler(eventQueue_t* event);
int32_t getLastFilteredSensorReading(sensorType_t sensor);
void setTareMark(int16_t sensorIndex);

void toggleCsvForDebug(void);
bool shouldShowCsvForDebug(void);

void toggleTareStateForDebug(void);
bool shouldShowTareStateForDebug(void);
