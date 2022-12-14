
#define _CRT_SECURE_NO_WARNINGS

#if  !defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include "Arduino.h"

extern HardwareSerial& dbgSerial;
extern HardwareSerial& nextionSerial;
#else
#include <string>
#include "win32shims.h"
#include "WString.h"

static  HardwareSerial      dbgSerial,
                            nextionSerial;
#endif //   !defined(WIN32)DEBUG

#define     _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "events.h"
#include "accumulationMgr.h"


static bool             accumulationScreenIsActive = false;
static stickState_t     currentStickStats;

/*  Binning in the QR code will be as follows:

   0    Light bin
   1    100 to 110 kg/m3
   2    110 to 150 kg/m3
   3    150 to 190 kg/m3
   4    190 to 230 kg/m3
   5    230 to 240 kg/m3
   6    Heavy bin

    The cummulations will be kept for each bin individually, only
    lumped together to make the QR code.

    Each QR code will include:
            count
            BFt
            Weight
            Length
*/

void accumulationObj_ReportNewStickStats(const stickState_t *statPtr)
{
    dbgSerial.print(F("ReportNewStickStats() entry\n"));

    dbgSerial.print(F("startStats:\n"));
    accumulationObj_ShowStickStats(&currentStickStats);
    dbgSerial.print(F("\n\n"));

    //  Add in all the new stats
    currentStickStats.weightSum_light      += statPtr->weightSum_light;
    currentStickStats.weightSum_heavy      += statPtr->weightSum_heavy;
    currentStickStats.weightSum_good       += statPtr->weightSum_good;
    currentStickStats.stickLengthSum_light += statPtr->stickLengthSum_light;
    currentStickStats.stickLengthSum_heavy += statPtr->stickLengthSum_heavy;
    currentStickStats.stickLengthSum_good  += statPtr->stickLengthSum_good;
    currentStickStats.stickCount_light     += statPtr->stickCount_light;
    currentStickStats.stickCount_heavy     += statPtr->stickCount_heavy;
    currentStickStats.stickCount_good      += statPtr->stickCount_good;
    currentStickStats.boardFeetSum_light   += statPtr->boardFeetSum_light;
    currentStickStats.boardFeetSum_heavy   += statPtr->boardFeetSum_heavy;
    currentStickStats.boardFeetSum_good    += statPtr->boardFeetSum_good;

    for (int index = 0; index < MAX_ACCUM_BINS; index++)
    {
        currentStickStats.boardFeetSum_binned  [index] += statPtr->boardFeetSum_binned  [index];
        currentStickStats.stickCount_binned    [index] += statPtr->stickCount_binned    [index];
        currentStickStats.stickLengthSum_binned[index] += statPtr->stickLengthSum_binned[index];
        currentStickStats.weightSum_binned     [index] += statPtr->weightSum_binned     [index];
    }

    //  Update the displayed values
    showAccumulationScreenAllValues();

    dbgSerial.print(F("BFt[8]="));
    dbgSerial.print(currentStickStats.boardFeetSum_binned[8]);
    dbgSerial.print(F("\n"));

    dbgSerial.print(F("Length[8]="));
    dbgSerial.print(currentStickStats.stickLengthSum_binned[8]);
    dbgSerial.print(F("\n"));

    dbgSerial.print(F("Weight[8]="));
    dbgSerial.print(currentStickStats.weightSum_binned[8]);
    dbgSerial.print(F("\n"));

    dbgSerial.print(F("Count[8]="));
    dbgSerial.print(currentStickStats.stickLengthSum_binned[8]);
    dbgSerial.print(F("\n"));

    dbgSerial.print(F("\nendStats:\n"));
    accumulationObj_ShowStickStats(&currentStickStats);
    dbgSerial.print(F("\n\n"));
}


void accumulationObj_EnableAccumulationScreen(void)
{
    accumulationScreenIsActive = true;
    showAccumulationScreenAllValues();
}


void accumulationObj_DisableAccumulationScreen(void)
{
    accumulationScreenIsActive = false;
}


void accumulationObj_ClearAccumulationScreen(void)
{
    dbgSerial.print(F("ClearAccumulationScreen\n"));
    memset(&currentStickStats, 0, sizeof(currentStickStats));
    showAccumulationScreenAllValues();
}


void writeFpValueToString(float fltVal, int length, const char *destPtr)
{
    int     decimalCnt = 0;

         if (fltVal ==     0.0f) decimalCnt = length - 2;
    else if (fltVal  <    10.0f) decimalCnt = length - 2;
    else if (fltVal  <   100.0f) decimalCnt = length - 3;
    else if (fltVal  <  1000.0f) decimalCnt = length - 4;
    else if (fltVal  < 10000.0f) decimalCnt = length - 5;
    else                         decimalCnt = length - 6;
    if (length < 0) length = 0;

    dtostrf(fltVal, length, decimalCnt, destPtr);
}


//  This string should be at least 192+1 characters long to hold the entire QR code
void makeQrCodeAccumulationString(char *accumulationString)
{
    int32_t tmpBft,
            tmpLength,
            tmpCount,
            tmpWeight,
            index;

    char    *fillPtr = accumulationString;

    float   myFloat;

    //  Fill 7 slots with all binned accumulations
    for (index = 0; index < 7; index++)
    {
        tmpBft    = 0;
        tmpLength = 0;
        tmpCount  = 0;
        tmpWeight = 0;
        switch (index)
        {
        case 0:     //  light sticks
            tmpBft    = currentStickStats.boardFeetSum_binned  [9 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[9 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [9 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [9 - 1];
            break;

        case 1:     //  100 to 110
            tmpBft    = currentStickStats.boardFeetSum_binned  [1 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[1 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [1 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [1 - 1];
            break;

        case 2:     //  110 to 150
            tmpBft    = currentStickStats.boardFeetSum_binned  [2 - 1] + currentStickStats.boardFeetSum_binned  [3 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[2 - 1] + currentStickStats.stickLengthSum_binned[3 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [2 - 1] + currentStickStats.stickCount_binned    [3 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [2 - 1] + currentStickStats.weightSum_binned     [3 - 1];
            break;

        case 3:     //  150 to 190
            tmpBft    = currentStickStats.boardFeetSum_binned  [4 - 1] + currentStickStats.boardFeetSum_binned  [5 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[4 - 1] + currentStickStats.stickLengthSum_binned[5 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [4 - 1] + currentStickStats.stickCount_binned    [5 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [4 - 1] + currentStickStats.weightSum_binned     [5 - 1];
            break;

        case 4:     //  190 to 230
            tmpBft    = currentStickStats.boardFeetSum_binned  [6 - 1] + currentStickStats.boardFeetSum_binned  [7 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[6 - 1] + currentStickStats.stickLengthSum_binned[7 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [6 - 1] + currentStickStats.stickCount_binned    [7 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [6 - 1] + currentStickStats.weightSum_binned     [7 - 1];
            break;

        case 5:     //  230 to 240
            tmpBft    = currentStickStats.boardFeetSum_binned  [8 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[8 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [8 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [8 - 1];
            break;

        case 6:     //  Heavy sticks
            tmpBft    = currentStickStats.boardFeetSum_binned  [10 - 1];
            tmpLength = currentStickStats.stickLengthSum_binned[10 - 1];
            tmpCount  = currentStickStats.stickCount_binned    [10 - 1];
            tmpWeight = currentStickStats.weightSum_binned     [10 - 1];
            break;
        }

        //  Print the stick count
        sprintf(fillPtr, "%d,", tmpCount);
        while (*fillPtr) fillPtr++;

        //  Convert grams to kg
        sprintf(fillPtr, "%d,", tmpWeight / 1000);
        while (*fillPtr) fillPtr++;


        //  Convert mm^3 to BFt
        myFloat = (float)tmpBft / 2359737.225974f;
        writeFpValueToString(myFloat, 6, fillPtr);

        dbgSerial.print(F("BFt value="));
        dbgSerial.print(tmpBft);
        dbgSerial.print(F(" "));
        dbgSerial.print((float)myFloat);
        dbgSerial.print(fillPtr);

        while (*fillPtr) fillPtr++;
        strcpy(fillPtr, ",");
        while (*fillPtr) fillPtr++;

        //  Convert mm to meters of length
        myFloat = (float)tmpLength / 1000.0f;
        writeFpValueToString(myFloat, 5, fillPtr);

        while (*fillPtr) fillPtr++;
    }
}


void showMainScreenBftValue(void)
{
    int32_t     bftSum = currentStickStats.boardFeetSum_good  +
                         currentStickStats.boardFeetSum_light +
                         currentStickStats.boardFeetSum_heavy;

    float       bft_float = (float)bftSum / 2359737.225974f;    //  Convert mm^3 to BFt

    char        displayString[20];

    dtostrf(bft_float, 6, 1, displayString);

    nextionSerial.print(F("bft.txt=\""));
    nextionSerial.print(displayString);
    nextionSerial.print(F("\"\xFF\xFF\xFF"));

    dbgSerial.print(F("bftSum="));
    dbgSerial.print(bftSum);
    dbgSerial.print(F("  float="));
    dbgSerial.print(bft_float);
    dbgSerial.print(F("  displayString="));
    dbgSerial.print(displayString);
    dbgSerial.print(F("\xFF\xFF\xFF"));

    dbgSerial.print(F("bft.txt=\""));
    dbgSerial.print(displayString);
    dbgSerial.print(F("\"\xFF\xFF\xFF"));
    dbgSerial.print(F("\n"));
}


void showAccumulationScreenAllValues(void)
{
    char        displayString[30];

    int32_t     tmpBft,
                tmpLength,
                tmpCount,
                tmpWeight,
                index;

    float       myFloat;

    dbgSerial.print(F("showAccumulationScreenAllValues() entry\n"));
    if (!accumulationScreenIsActive)
    {   //  There is nothing to show, we are on the wrong screen
        return;
    }

    dbgSerial.print(F("showAccumulationScreenAllValues() continues\n"));
    //  Fill 3 slots with light/good/heavy binned accumulations
    for (index = 0; index < 3; index++)
    {
        switch (index)
        {
        case 0:     //  light sticks
            tmpBft    = currentStickStats.boardFeetSum_light;
            tmpLength = currentStickStats.stickLengthSum_light;
            tmpCount  = currentStickStats.stickCount_light;
            tmpWeight = currentStickStats.weightSum_light;
            break;

        case 1:     //  good sticks
            tmpBft    = currentStickStats.boardFeetSum_good;
            tmpLength = currentStickStats.stickLengthSum_good;
            tmpCount  = currentStickStats.stickCount_good;
            tmpWeight = currentStickStats.weightSum_good;
            break;

        case 2:     //  heavy sticks
            tmpBft    = currentStickStats.boardFeetSum_heavy;
            tmpLength = currentStickStats.stickLengthSum_heavy;
            tmpCount  = currentStickStats.stickCount_heavy;
            tmpWeight = currentStickStats.weightSum_heavy;
            break;
        }

        //  Print the stick count
        sprintf(displayString, "%d", tmpCount);

        switch (index)
        {
        case 0: nextionSerial.print(F("ltCnt.txt=\""   )); dbgSerial.print(F("ltCnt.txt=\""   )); break;
        case 1: nextionSerial.print(F("goodCnt.txt=\"" )); dbgSerial.print(F("goodCnt.txt=\"" )); break;
        case 2: nextionSerial.print(F("heavyCnt.txt=\"")); dbgSerial.print(F("heavyCnt.txt=\"")); break;
        }
        nextionSerial.print(displayString);       dbgSerial.print(displayString);
        nextionSerial.print(F("\"\xFF\xFF\xFF")); dbgSerial.print(F("\"\n"));

        //  Convert grams to kg
        myFloat = (float)tmpWeight / 1000.0;
        if (myFloat < 10.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 100.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 1000.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 10000.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else
        {
            dtostrf(myFloat, 6, 0, displayString);
        }

        switch (index)
        {
        case 0: nextionSerial.print(F("ltWeight.txt=\""   )); break;
        case 1: nextionSerial.print(F("goodWeight.txt=\"" )); break;
        case 2: nextionSerial.print(F("heavyWeight.txt=\"")); break;
        }
        nextionSerial.print(displayString);
        nextionSerial.print(F("\"\xFF\xFF\xFF"));

        //  Convert mm^3 to BFt
        myFloat = (float)tmpBft / 2359737.225974f;
        if (myFloat < 10.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 100.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 1000.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpBft < 10000.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else
        {
            dtostrf(myFloat, 6, 0, displayString);
        }
        switch (index)
        {
        case 0: nextionSerial.print(F("ltVolume.txt=\""   )); dbgSerial.print(F("ltVolume.txt=\""   )); break;
        case 1: nextionSerial.print(F("goodVolume.txt=\"" )); dbgSerial.print(F("goodVolume.txt=\"" )); break;
        case 2: nextionSerial.print(F("heavyVolume.txt=\"")); dbgSerial.print(F("heavyVolume.txt=\"")); break;
        }
        nextionSerial.print(displayString);       dbgSerial.print(displayString);
        nextionSerial.print(F("\"\xFF\xFF\xFF")); dbgSerial.print(F("\"\n"));

        //  Convert mm to meters of length
        myFloat = (float)tmpLength / 1000.0f;
        if (myFloat < 10.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpLength < 100.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else if (tmpLength < 1000.0f)
        {
            dtostrf(myFloat, 6, 1, displayString);
        }
        else
        {
            dtostrf(myFloat, 6, 0, displayString);
        }
        switch (index)
        {
        case 0: nextionSerial.print(F("ltLength.txt=\""   )); dbgSerial.print(F("ltLength.txt=\""   )); break;
        case 1: nextionSerial.print(F("goodLength.txt=\"" )); dbgSerial.print(F("goodLength.txt=\"" )); break;
        case 2: nextionSerial.print(F("heavyLength.txt=\"")); dbgSerial.print(F("heavyLength.txt=\"")); break;
        }
        nextionSerial.print(displayString);         dbgSerial.print(displayString);
        nextionSerial.print(F("\"\xFF\xFF\xFF"));   dbgSerial.print(F("\"\n"));
    }

    //  Now send the QR code
    char    qrCodeStringBuffer[200];
    qrCodeStringBuffer[200];

    makeQrCodeAccumulationString(qrCodeStringBuffer);

    nextionSerial.print(F("qr.txt=\""));
    nextionSerial.print(qrCodeStringBuffer);
    nextionSerial.print(F("\"\xFF\xFF\xFF"));

    dbgSerial.print(F("qr.txt=\""));
    dbgSerial.print(qrCodeStringBuffer);
    dbgSerial.print(F("\"\n"));
}


void accumulationObj_Initialize(void)
{
}


void accumulationObj_EventHandler(eventQueue_t* event)
{
}

void accumulationObj_ShowStickStats(stickState_t *statPtr)
{
    for (int index = 0; index < MAX_ACCUM_BINS; index++)
    {
        dbgSerial.print(F("["));
        dbgSerial.print(index);
        dbgSerial.print(F("] Cnt="));
        dbgSerial.print(statPtr->stickCount_binned[index]);
        dbgSerial.print(F("  length="));
        dbgSerial.print(statPtr->stickLengthSum_binned[index]);
        dbgSerial.print(F("  Bft="));
        dbgSerial.print(statPtr->boardFeetSum_binned [index]);
        dbgSerial.print(F("  Weight="));
        dbgSerial.print(statPtr->weightSum_binned[index]);
        dbgSerial.print(F("\n"));
    }
}
