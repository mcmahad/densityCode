
#define _CRT_SECURE_NO_WARNINGS

#if  !defined(_WIN32)
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
        currentStickStats.boardFeetSum_binned  [index] += statPtr->boardFeetSum_binned[index];
        currentStickStats.stickCount_binned    [index] += statPtr->stickCount_binned[index];
        currentStickStats.stickLengthSum_binned[index] += statPtr->stickLengthSum_binned[index];
        currentStickStats.weightSum_binned     [index] += statPtr->weightSum_binned[index];
    }
}


//  This string should be at least 192+1 characters long to hold the entire QR code
void makeQrCodeAccumulationString(char *accumulationString)
{
    int     tmpBft,
            tmpLength,
            tmpCount,
            tmpWeight,
            index;

    char    *fillPtr = accumulationString;

    float   myFloat;

    //  Fill 7 slots with all binned accumulations
    for (index = 0; index < 7; index++)
    {
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
        if (myFloat < 10.0f)
        {
            sprintf(fillPtr, "%6.4f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 100.0f)
        {
            sprintf(fillPtr, "%6.3f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 1000.0f)
        {
            sprintf(fillPtr, "%6.2f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 10000.0f)
        {
            sprintf(fillPtr, "%6.1f,", myFloat);      //  Convert mm^3 to BFt
        }
        else
        {
            sprintf(fillPtr, "%6.0f,", myFloat);      //  Convert mm^3 to BFt
        }
        while (*fillPtr) fillPtr++;

        
        //  Convert mm to meters of length
        myFloat = (float)tmpLength / 1000.0f;
        if (myFloat < 10.0f)
        {
            sprintf(fillPtr, "%5.3f,", myFloat);     //  Convert mm to meters of length
        }
        else if (tmpLength < 100.0f)
        {
            sprintf(fillPtr, "%5.2f,", myFloat);     //  Convert mm to meters of length
        }
        else if (tmpLength < 1000.0f)
        {
            sprintf(fillPtr, "%5.1f,", myFloat);     //  Convert mm to meters of length
        }
        else
        {
            sprintf(fillPtr, "%5.0f,", myFloat);     //  Convert mm to meters of length
        }
        while (*fillPtr) fillPtr++;
    }

    //  Back up one character at the end and make it a newline
    fillPtr--;
    *fillPtr = '\n';
}


void showMainScreenBftValue(void)
{
    int32_t     bftSum = currentStickStats.boardFeetSum_good  +
                         currentStickStats.boardFeetSum_light +
                         currentStickStats.boardFeetSum_heavy;

    float       bft_float = (float)bftSum / 2359737.225974f;    //  Convert mm^3 to BFt

    char        displayString[30];

    if (bft_float < 10.0f)
    {
        sprintf(displayString, "%6.4f,", bft_float);    //  Convert mm^3 to BFt
    }
    else if (bft_float < 100.0f)
    {
        sprintf(displayString, "%6.3f,", bft_float);   //  Convert mm^3 to BFt
    }
    else if (bft_float < 1000.0f)
    {
        sprintf(displayString, "%6.2f,", bft_float);   //  Convert mm^3 to BFt
    }
    else if (bft_float < 10000.0f)
    {
        sprintf(displayString, "%6.1f,", bft_float);   //  Convert mm^3 to BFt
    }
    else
    {
        sprintf(displayString, "%6.0f,", bft_float);   //  Convert mm^3 to BFt
    }

    nextionSerial.print(F("bft.val="));
    nextionSerial.print(displayString);
    nextionSerial.print(F("\xFF\xFF\xFF"));

    //  Now send the QR code
    char    qrCodeStringBuffer[200];
    qrCodeStringBuffer[200];

    makeQrCodeAccumulationString(qrCodeStringBuffer);

    nextionSerial.print(F("qr.txt="));
    nextionSerial.print(qrCodeStringBuffer);
    nextionSerial.print(F("\xFF\xFF\xFF"));
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
        sprintf(displayString, "%d,", tmpCount);
        
        switch (index)
        {
        case 0: nextionSerial.print(F("ltCnt.val="   )); break;
        case 1: nextionSerial.print(F("goodCnt.val=" )); break;
        case 2: nextionSerial.print(F("heavyCnt.val=")); break;
        }
        nextionSerial.print(displayString);
        nextionSerial.print(F("\xFF\xFF\xFF"));


        //  Convert grams to kg
        sprintf(displayString, "%d,", tmpWeight / 1000);
        switch (index)
        {
        case 0: nextionSerial.print(F("ltWeight.val="   )); break;
        case 1: nextionSerial.print(F("goodWeight.val=" )); break;
        case 2: nextionSerial.print(F("heavyWeight.val=")); break;
        }
        nextionSerial.print(displayString);
        nextionSerial.print(F("\xFF\xFF\xFF"));


        //  Convert mm^3 to BFt
        myFloat = (float)tmpBft / 2359737.225974f;
        if (myFloat < 10.0f)
        {
            sprintf(displayString, "%6.4f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 100.0f)
        {
            sprintf(displayString, "%6.3f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 1000.0f)
        {
            sprintf(displayString, "%6.2f,", myFloat);      //  Convert mm^3 to BFt
        }
        else if (tmpBft < 10000.0f)
        {
            sprintf(displayString, "%6.1f,", myFloat);      //  Convert mm^3 to BFt
        }
        else
        {
            sprintf(displayString, "%6.0f,", myFloat);      //  Convert mm^3 to BFt
        }
        switch (index)
        {
        case 0: nextionSerial.print(F("ltVolume.val="   )); break;
        case 1: nextionSerial.print(F("goodVolume.val=" )); break;
        case 2: nextionSerial.print(F("heavyVolume.val=")); break;
        }
        nextionSerial.print(displayString);
        nextionSerial.print(F("\xFF\xFF\xFF"));

        
        //  Convert mm to meters of length
        myFloat = (float)tmpLength / 1000.0f;
        if (myFloat < 10.0f)
        {
            sprintf(displayString, "%5.3f,", myFloat);     //  Convert mm to meters of length
        }
        else if (tmpLength < 100.0f)
        {
            sprintf(displayString, "%5.2f,", myFloat);     //  Convert mm to meters of length
        }
        else if (tmpLength < 1000.0f)
        {
            sprintf(displayString, "%5.1f,", myFloat);     //  Convert mm to meters of length
        }
        else
        {
            sprintf(displayString, "%5.0f,", myFloat);     //  Convert mm to meters of length
        }
        switch (index)
        {
        case 0: nextionSerial.print(F("ltLength.val="   )); break;
        case 1: nextionSerial.print(F("goodLength.val=" )); break;
        case 2: nextionSerial.print(F("heavyLength.val=")); break;
        }
        nextionSerial.print(displayString);
        nextionSerial.print(F("\xFF\xFF\xFF"));
    }
}


void accumulationObj_Initialize(void)
{
}


void accumulationObj_EventHandler(eventQueue_t* event)
{
}
