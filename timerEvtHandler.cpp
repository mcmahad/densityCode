

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef _WIN32
#include <arduino.h>
#else
#include "win32shims.h"
#endif // !_WIN32

#include "events.h"
#include "timerEvtHandler.h"

#ifdef WIN32
#include "arduinoStubs.h"
#endif // WIN32

#define   MAX_TIMER_EVTS    12

#ifdef _WIN32
#include "win32shims.h"
static  HardwareSerial  dbgSerial,
                        nextionSerial;
#else
extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;
#endif // _WIN32

typedef struct
{
    event_t     timeoutEvent;
    uint32_t    duration,
                startTime;
    int8_t      data1,
                data2;
    bool        isPeriodic;
} timerQueue_t;

static  timerQueue_t    timerQueue[MAX_TIMER_EVTS];

static  uint8_t         activeTimerCnt,
                        maxTimerCnt;


void timerObj_Initialize(void)
{
    int16_t         index;
    timerQueue_t    *timerPtr = timerQueue;

    for (index = 0; index < MAX_TIMER_EVTS; index++)
    {
        timerPtr->startTime    = 0;
        timerPtr->data1        = 0;
        timerPtr->data2        = 0;
        timerPtr->duration     = 0;
        timerPtr->isPeriodic   = false;
        timerPtr->timeoutEvent = systemEvt_nullEvt;
        timerPtr++;
    }
    activeTimerCnt = 0;
    maxTimerCnt    = 0;
}


static void startTimer(event_t eventToSend, uint32_t timerDuration, bool isPeriodic)
{
    timerQueue_t    *timerPtr = timerQueue;

    if (timerDuration == 0)
    {
        //  Never let the duration be zero, it confuses the logic for active timers
        dbgSerial.print(F("Duration is zero  Event=0x"));
        dbgSerial.println(eventToSend, HEX);
        timerDuration = 1;
    }

    if (activeTimerCnt < MAX_TIMER_EVTS)
    {
        while (timerPtr != &timerQueue[MAX_TIMER_EVTS])
        {
            /*  Find the first empty timer slot and insert the new timer  */
            if (timerPtr->duration == 0)
            {
                timerPtr->startTime    = getTime_mSec();
                timerPtr->duration     = timerDuration;
                timerPtr->isPeriodic   = isPeriodic;
                timerPtr->timeoutEvent = eventToSend;
                timerPtr->data1        = 0;
                timerPtr->data2        = 0;
                activeTimerCnt++;
                if (maxTimerCnt < activeTimerCnt)
                {
                    maxTimerCnt = activeTimerCnt;
                }
                break;
            }
            timerPtr++;
        }
    }
    else
    {
        for (int index = 0; index < 1; index++)
        {
            dbgSerial.print(F("\n\nERROR - No Timers "));
            dbgSerial.print(eventToSend, HEX);
            dbgSerial.println();

            dbgSerial.println(F("Active timers : "));
            for (int count = 0; count < MAX_TIMER_EVTS; count++)
            {
                dbgSerial.print(timerQueue[count].timeoutEvent, HEX);
                dbgSerial.print(F("  "));
            }
        }
    }
}


void cancelTimer(event_t eventId)
{
    timerQueue_t    *timerPtr = timerQueue;

    while (timerPtr != &timerQueue[MAX_TIMER_EVTS])
    {
        if (timerPtr->timeoutEvent == eventId)
        {
            timerPtr->startTime    = 0;
            timerPtr->duration     = 0;
            timerPtr->isPeriodic   = false;
            timerPtr->timeoutEvent = systemEvt_nullEvt;
            activeTimerCnt--;
        }
        timerPtr++;
    }
    cancelQueuedTimerStartEvents(eventId);
}


int16_t timerActiveCount(void)
{
    /*  Returns the number of active timers.  */
    return activeTimerCnt;
}


void setTimerData1(event_t eventToChange, int8_t newData1)
{
    timerQueue_t    *timerPtr = timerQueue;

    while (timerPtr != &timerQueue[MAX_TIMER_EVTS])
    {
        if (timerPtr->timeoutEvent == eventToChange)
        {
            timerPtr->data1 = newData1;
        }
        timerPtr++;
    }
}


void setTimerData2(event_t eventToChange, int8_t newData2)
{
    timerQueue_t    *timerPtr = timerQueue;

    while (timerPtr != &timerQueue[MAX_TIMER_EVTS])
    {
        if (timerPtr->timeoutEvent == eventToChange)
        {
            timerPtr->data2 = newData2;
        }
        timerPtr++;
    }
}


void showAllTimers(void)
{
    dbgSerial.print(F("Active timers : "));
    dbgSerial.println(activeTimerCnt);

    for (int count = 0; count < MAX_TIMER_EVTS; count++)
    {
        if (timerQueue[count].duration != 0)
        {
#ifndef _WIN32
            dbgSerial.print(F("0x"));
            dbgSerial.print(timerQueue[count].timeoutEvent, HEX);
            dbgSerial.print(F("  dur="));
            dbgSerial.print(timerQueue[count].duration, DEC);
            dbgSerial.print(F("  data1="));
            dbgSerial.print(timerQueue[count].data1, DEC);
            dbgSerial.print(F("  data2="));
            dbgSerial.print(timerQueue[count].data2, DEC);
            dbgSerial.println();
#endif // !_WIN32
        }
    }
    dbgSerial.println();
}


void timerObj_EventHandler(eventQueue_t* event)
{
    switch (event->eventId)
    {
    case timerEvt_startTimer:
        /*  Data1 is event ID to cancel
            Data2 is timer duration
        */
        startTimer((event_t)event->data1, event->data2, false);
//      dbgSerial.print(F("StartTimer\n"));
        break;

    case timerEvt_startPeriodicTimer:
        /*  Data1 is event ID to cancel
            Data2 is timer duration
        */
        startTimer((event_t)event->data1, event->data2, true);
//      dbgSerial.print(F("StartPeriodicTimer\n"));
        break;

    case timerEvt_cancelTimer:
        /*  Data1 is event ID to cancel
        */
        cancelTimer((event_t)event->data1);
//      dbgSerial.print(F("cancelTimer\n"));
        break;

    case timerEvt_setTimerData1:
        setTimerData1((event_t)event->data1, event->data2);
        break;

    case timerEvt_setTimerData2:
        setTimerData2((event_t)event->data1, event->data2);
        break;

    default:
        break;
    }
}


void updateTimerEvents(void)
{
    uint32_t    now = getTime_mSec();

    timerQueue_t    *timerPtr = timerQueue;

    int16_t         index,
                    remainingTimersCnt = activeTimerCnt;

    for (index = 0; index < MAX_TIMER_EVTS && remainingTimersCnt != 0; index++)
    {
        if (timerPtr->duration != 0)
        {
            /*  Determine if this timer has expired.  If so, send the associated event  */
            if (now - (timerPtr->startTime + timerPtr->duration) < 0x80000000L)
            {
                /*  This timer has expired.  Send the event  */
                sendEvent(timerPtr->timeoutEvent, timerPtr->data1, timerPtr->data2);

                if (timerPtr->isPeriodic)
                {
                    /*  Restart the periodic timer by updating the start time  */
                    timerPtr->startTime += timerPtr->duration;
                }
                else
                {
                    /*  Return this timer to the free pool  */
                    timerPtr->startTime    = 0;
                    timerPtr->duration     = 0;
                    timerPtr->isPeriodic   = false;
                    timerPtr->timeoutEvent = systemEvt_nullEvt;
                    activeTimerCnt--;
                }
            }
            remainingTimersCnt--;
        }
        timerPtr++;
    }
}


// ------------------------------------------- //
//                                             //
//  Time related functions for use by all.     //
//                                             //
// ------------------------------------------- //

uint32_t  getTime_mSec(void)
{
    /*  This is the core function to return time for all functions
    *   in the system.  This funtion returns time with only 31 bit
    *   precision because it is easier to deal with time values less
    *   than 32-bits without worrying about wrap-around and we sometimes
    *   need to add two time values that could potentially wrap with 32-bits.
    */
    return millis();
}



uint32_t getTimeDiff_mSec(uint32_t startTime, uint32_t endTime)
{
    /*
    *  Return endTime - startTime and take account into wrapping.
    */

    return (startTime <= endTime) ? (endTime - startTime) : ((0xFFFFFFFF - startTime + endTime) + 1);
}
