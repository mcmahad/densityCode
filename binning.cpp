
#ifndef _WIN32
#include <arduino.h>
#else
#include "math.h"
#include "win32shims.h"
#endif // _WIN32

#include "events.h"
#include "binning.h"

#ifndef _WIN32
extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;
#else
static  HardwareSerial      dbgSerial,
                            nextionSerial;

#endif // _WIN32




void binningObj_Initialize(void)
{
}

static  int32_t curentStickLength_mm,
                curentStickWidth_mm,
                curentStickHeight_mm,
                curentStickWeight_grams;

static  float   curentStickDensity_kgm3;

void binningObj_EventHandler(eventQueue_t* event)
{
//  dbgSerial.println(F("binning:"));

    switch (event->eventId)
    {
    case binningEvt_SetCurrentStickLength:
        curentStickLength_mm = event->data1;
        break;

    case binningEvt_SetCurrentStickWidth:
        curentStickWidth_mm = event->data1;
        break;

    case binningEvt_SetCurrentStickHeight:
        curentStickHeight_mm = event->data1;
        break;

    case binningEvt_SetCurrentStickWeight:
        curentStickWeight_grams = event->data1;
        break;

    case binningEvt_SetCurrentStickDensity:
        curentStickDensity_kgm3 = (float)event->data1;
        break;

    case binningEvt_ShowDensityBall:
        //  data1 : density x10, truncated to integer
        {
            int     ballIndex = 0;
            /*  Ball  -  Range
                 1        100 .. 110
                 2        110 .. 130
                 3        130 .. 150
                 4        150 .. 170
                 5        170 .. 190
                 6        190 .. 210
                 7        210 .. 230
                 8        230 .. 240
            */

            /*  This is where we determine how to move in the acceptable limits to account for
                measurement inaccuracy.

                Each measurement (l/h/w/wt) is assigned an uncertainty value, like 2.3mm or 1.6grams.
                The outer limits are sccaled in by the pctg found from uncertainty/current measurement
                and each dimension is multiplied in.
            */

            const   float   lengthUncertainty  = 2.3f,   // mm
                            widthUncertainty   = 1.8f,   // mm
                            heightUncertainty  = 1.8f,   // mm
                            weightUncertainty  = 2.5f;   // grams

            float           densityLimitScaling =       //  A percentage to encroach the outer limits
                                                 ((1.0f + lengthUncertainty  / (float)curentStickLength_mm   )
                                                * (1.0f + widthUncertainty   / (float)curentStickWidth_mm    )
                                                * (1.0f + heightUncertainty  / (float)curentStickHeight_mm   )
                                                * (1.0f + weightUncertainty  / (float)curentStickWeight_grams) - 1.0f);

#ifndef _WIN32
            dbgSerial.println(F("ShowBall event"));
#endif _WIN32


            //  First, apply the encroached outer limits we found above.  If inside those limits, then apply the old rules
            densityLimitScaling = (float)fabs(densityLimitScaling);
            if (densityLimitScaling > 0.10) densityLimitScaling = 0.10f;    //  Keep it sane
            if (densityLimitScaling < 0.00) densityLimitScaling = 0.00f;

            //  Check encroachment for outer limits of 100 and 240 kg/m3
                 if (curentStickDensity_kgm3 < 100.0 * (1.0 + densityLimitScaling)) ballIndex =  9;
            else if (curentStickDensity_kgm3 > 240.0 * (1.0 - densityLimitScaling)) ballIndex = 10;

            //  Check all the rest of the limits
            else if (event->data1 >= 1000  &&  event->data1 <= 1100) ballIndex =  1;
            else if (event->data1 >= 1100  &&  event->data1 <= 1300) ballIndex =  2;
            else if (event->data1 >= 1300  &&  event->data1 <= 1500) ballIndex =  3;
            else if (event->data1 >= 1500  &&  event->data1 <= 1700) ballIndex =  4;
            else if (event->data1 >= 1700  &&  event->data1 <= 1900) ballIndex =  5;
            else if (event->data1 >= 1900  &&  event->data1 <= 2100) ballIndex =  6;
            else if (event->data1 >= 2100  &&  event->data1 <= 2300) ballIndex =  7;
            else if (event->data1 >= 2300  &&  event->data1 <= 2400) ballIndex =  8;
            else if (event->data1 <= 1000                          ) ballIndex =  9;
            else if (                          event->data1 >= 2400) ballIndex = 10;

#ifndef _WIN32
            dbgSerial.print(F("ballIndex="));
            dbgSerial.println(ballIndex);

            if (ballIndex >= 1  &&  ballIndex <= 10)
            {
                //  7 is the magic index where ball pictures start in the Nextion
                nextionSerial.print(F("="));
                nextionSerial.print(ballIndex + 7);
                nextionSerial.print(F("\xFF\xFF\xFF"));
            }
            else
            {
                nextionSerial.print(F("ball.pic=7"));
                nextionSerial.print(F("\xFF\xFF\xFF"));

                dbgSerial.println(F("Empty ballIndex #1"));
            }
#else // !_WIN32
            printf(  "%f"       //  densityLimitScaling
                    ",%f"       //  lengthUncertainty
                    ",%d"       //  currentStickLength_mm
                    ",%d"       //  currentStickWdith_mm
                    ",%d"       //  currentStickHeight_mm
                    ",%d"       //  currentStickWeight_grams
                    ",%f"       //  lengthUncertainty
                    ",%f"       //  widthUncertainty
                    ",%f"       //  heightUncertainty
                    ",%f"       //  weigthUncertainty
                    ",%d"       //  ballIndex
                    "\n",

            densityLimitScaling,
            lengthUncertainty,
            curentStickLength_mm,
            curentStickWidth_mm,
            curentStickHeight_mm,
            curentStickWeight_grams,

            widthUncertainty,
            heightUncertainty,
            weightUncertainty,
            densityLimitScaling,
            ballIndex
                );

#endif // WIN32

        }
        break;

    case binningEvt_ClearDensityBall:
//      dbgSerial.println(F("clearBall event"));
        nextionSerial.print(F("=7"));
        nextionSerial.print(F("\xFF\xFF\xFF"));

//      dbgSerial.println(F("Empty ballIndex #2"));
        break;

    default:
        dbgSerial.print(F("unknown event "));
        nextionSerial.println(event->eventId);
        break;
    }
}

