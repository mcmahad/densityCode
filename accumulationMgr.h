

#pragma once

#include "events.h"


#define     MAX_ACCUM_BINS      (10)

typedef struct
{
    //  First, all the higher level important stats
    int32_t     stickCount_light,       //  counts
                stickLengthSum_light,   //  mm
                boardFeetSum_light,     //  mm^3
                weightSum_light,        //  Grams
                stickCount_good,        //  counts
                stickLengthSum_good,    //  mm
                boardFeetSum_good,      //  mm^3
                weightSum_good,         //  Grams
                stickCount_heavy,       //  counts
                stickLengthSum_heavy,   //  mm
                boardFeetSum_heavy,     //  mm^3
                weightSum_heavy;        //  Grams

    //  Now all the details
    int32_t     stickCount_binned    [MAX_ACCUM_BINS],      //  counts
                stickLengthSum_binned[MAX_ACCUM_BINS],       // mm
                boardFeetSum_binned  [MAX_ACCUM_BINS],      //  mm^3
                weightSum_binned     [MAX_ACCUM_BINS];      // Grams

} stickState_t;

void accumulationObj_Initialize(void);
void accumulationObj_EventHandler(const eventQueue_t* event);

void accumulationObj_EnableAccumulationScreen(void);
void accumulationObj_DisableAccumulationScreen(void);
void accumulationObj_ClearAccumulationScreen(void);
void accumulationObj_ReportNewStickStats(const stickState_t *statPtr);
void accumulationObj_ResetStickStats(void);
void showMainScreenBftValue(void);
void showAccumulationScreenAllValues(void);
void accumulationObj_ShowStickStats(stickState_t *statPtr);
void *getAccumulationDataStructure(int16_t *accumDataSize);

