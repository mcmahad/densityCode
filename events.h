
#pragma once

#include <stdint.h>


typedef enum
{
    evtObj_System        = 0,
    evtObj_Timer            ,
    evtObj_LoadCell         ,
    evtObj_SerialRead       ,
    evtObj_MsmtMgr          ,
    evtObj_AdcCvt           ,
    evtObj_TallyTracker     ,
    evtObj_Binning          ,
    evtObj_Accumulation     ,
} eventObject_t;


#define     EVTS_PER_OBJECT         256
#define     OBJ2EVT_BASE(object)    ((unsigned int)object * EVTS_PER_OBJECT)
#define     EVT2OBJ(event)          ((unsigned int)event  / EVTS_PER_OBJECT)

typedef enum
{
    systemEvt_baseId = OBJ2EVT_BASE(evtObj_System),
    systemEvt_nullEvt,

    timerEvt_baseId = OBJ2EVT_BASE(evtObj_Timer),
    timerEvt_startTimer,
    timerEvt_startPeriodicTimer,
    timerEvt_cancelTimer,
    timerEvt_setTimerData1,
    timerEvt_setTimerData2,

    loadCellEvt_baseId = OBJ2EVT_BASE(evtObj_LoadCell),
    loadCellEvt_InitializeLoadCell,             //  Initialize the load cell
    loadCellEvt_PowerUpLoadCell,                // 0x0202
    loadCellEvt_WaitForLoadCellReady,           // 0x0203
    loadCellEvt_StartLoadCellCollection,        // 0x0204
    loadCellEvt_RestartLoadCell,                // 0x0205

    loadCellEvt_CalibrateWeight,
    loadCellEvt_CalibrateAmplifierOffset,
    loadCellEvt_SetFilterWeightingPctg,
    loadCellEvt_PrintCellWeight,
    loadCellEvt_ScreenTestTimeout,

    serialReadEvt_baseId = OBJ2EVT_BASE(evtObj_SerialRead),
    serialReadEvt_DataAvailable,
    serialReadEvt_ClearReceiveBuffer,
    serialReadEvt_CmdFlushTimeout,
    serialReadEvt_NullString,
    serialReadEvt_Ping,
    serialReadEvt_SetTare,
    serialReadEvt_IceCmd,
    serialReadEvt_RawCmd,
    serialReadEvt_CookCmd,
    serialReadEvt_RateCmd,
    serialReadEvt_CclrCmd,
    serialReadEvt_CsetCmd,
    serialReadEvt_GcalCmd,
    serialReadEvt_NoiseCmd,
    serialReadEvt_GraphCmd,
    serialReadEvt_QuantCmd,
    serialReadEvt_FinalCmd,
    serialReadEvt_FastFwdCmd,
    serialReadEvt_ToggleTareDebugState,
    serialReadEvt_ToggleLogDebugState,
    serialReadEvt_CheckBoardSignatureTimeout,
    serialReadEvt_RequestVersionInfo,
    serialReadEvt_TallyStatus,
    serialReadEvt_TallyValid,
    serialReadEvt_AccumScreenEnable,
    serialReadEvt_AccumScreenDisable,
    serialReadEvt_AccumScreenClear,

    msmtMgrEvt_baseId = OBJ2EVT_BASE(evtObj_MsmtMgr),
    msmtMgrEvt_SetReportRate,
    msmtMgrEvt_EnableRawReports,
    msmtMgrEvt_DisableRawReports,
    msmtMgrEvt_EnableFinalReports,
    msmtMgrEvt_MoveToLicenseScreen,
    msmtMgrEvt_DisableFinalReports,
    msmtMgrEvt_PeriodicReportTimeout,
    msmtMgrEvt_ReportRawSensorMsmt,
    msmtMgrEvt_ReportPing,
    msmtMgrEvt_SetNoiseMonitor,
    msmtMgrEvt_SetGraphIncrease,
    msmtMgrEvt_GraphScaleRecenter,
    msmtMgrEvt_GraphScaleIncrease,
    msmtMgrEvt_GraphScaleDecrease,
    msmtMgrEvt_QuantizationDelta,
    msmtMgrEvt_ReportCalibrationSet1,
    msmtMgrEvt_ReportCalibrationSet2,
    msmtMgrEvt_ForceDensityDisplayUpdate,
    msmtMgrEvt_ReportMainScreenAccumulations,

    adcCvtEvt_baseId = OBJ2EVT_BASE(evtObj_AdcCvt),
    adcCvtEvt_InitializeConverter,                    //  Initialize the entire ADC converter
    adcCvtEvt_WaitConverterReadyTimeout,              //  Timeout, ready to check if data is ready
    adcCvtEvt_RestartCurrentConversionTimeout,        //  Timeout, restart the conversion
    adcCvtEvt_BumpGain,                               //  Test only
    adcCvtEvt_WatchdogTimeout,                        //  Overriding watchdog timeout to make sure we don't stall

    tallyTrkEvt_baseId = OBJ2EVT_BASE(evtObj_TallyTracker),
    tallyTrkEvt_SetCountRemaining,
    tallyTrkEvt_DensityPublished,
    tallyTrkEvt_DensityCleared,
    tallyTrkEvt_CountOneStick,
    tallyTrkEvt_InactivityTimeout,
    tallyTrkEvt_SaveTallyToEeprom,
    tallyTrkEvt_CreateNewInventoryKey,
    tallyTrkEvt_TallyStatusEntry,
    tallyTrkEvt_TallyStatusExit,
    tallyTrkEvt_UpdateTallyStatusScreen,
    tallyTrkEvt_TallyValid,

    binningEvt_baseId = OBJ2EVT_BASE(evtObj_Binning),
    binningEvt_ShowDensityBall,
    binningEvt_ClearDensityBall,
    binningEvt_SetCurrentStickLength,
    binningEvt_SetCurrentStickWidth,
    binningEvt_SetCurrentStickHeight,
    binningEvt_SetCurrentStickWeight,
    binningEvt_SetCurrentStickDensity,

    accumulationEvt_baseId = OBJ2EVT_BASE(evtObj_Accumulation),
    accumulationEvt_LogNewStickInfo,
    accumulationEvt_ResetStickCounts,
} event_t;


typedef struct
{
    event_t     eventId;
    uint32_t    data1,
                data2;
} eventQueue_t;


void sendEvent(event_t eventId, int32_t data0, int32_t data1);
void showAllQueuedEvents(void);

void cancelTimer(event_t eventId);
int16_t timerActiveCount(void);
void showAllTimers(void);

void initializeEvents(void);
void processEvents(void);
