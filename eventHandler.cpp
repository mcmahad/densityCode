

#include <stdint.h>

#ifndef _WIN32
#include <arduino.h>
#else
#include "win32shims.h"
#endif // !_WIN32

#include "commonHeader.h"
#include "events.h"
#include "timerEvtHandler.h"
#include "loadCellEvtHandler.h"
#include "serialReadEvtHandler.h"
#include "msmtMgrEvtHandler.h"
#include "adcCvtEvtHandler.h"
#include "tallyTracker.h"
#include "binning.h"
#include "accumulationMgr.h"

#define MAX_EVENTS    20

eventQueue_t    eventQueue[MAX_EVENTS],
                *eventQueueHeadPtr = eventQueue,
                *eventQueueTailPtr = eventQueue;


//  #define DEBUG_ONLY
// #define ARDUINO_UNO
// #define ARDUINO_MEGA


#ifndef _WIN32

#define ARDUINO_MEGA

#if   defined(ARDUINO_UNO)
HardwareSerial  &dbgSerial     = Serial;
HardwareSerial  &nextionSerial = Serial;
#elif   defined(ARDUINO_MEGA)
HardwareSerial  &dbgSerial     = Serial;
HardwareSerial  &nextionSerial = Serial1;
#else
#warning Unknown serial port config
#endif

#else
HardwareSerial  dbgSerial;
HardwareSerial  nextionSerial;

#endif // !_WIN32


void showAllQueuedEvents(void)
{
    eventQueue_t    *tmpTailPtr = eventQueueTailPtr;
    int             index = 0;

    dbgSerial.println(F("Queued Events"));
    while (tmpTailPtr != eventQueueHeadPtr)
    {
        dbgSerial.print(index++);
        dbgSerial.print(F("  "));
        dbgSerial.print(tmpTailPtr->eventId, HEX);
        dbgSerial.print(F("  "));
        dbgSerial.print((int)tmpTailPtr->data1);
        dbgSerial.print(F("  "));
        dbgSerial.print((int)tmpTailPtr->data2);
        dbgSerial.println();

        tmpTailPtr++;
        if (tmpTailPtr >= &eventQueue[MAX_EVENTS])
        {
            tmpTailPtr = eventQueue;
        }
    }
}


void initEventQueue(void)
{
    int16_t index;

    for (index = 0; index < MAX_EVENTS; index++)
    {
        eventQueue[index].eventId = systemEvt_nullEvt;
        eventQueue[index].data1   = 0;
        eventQueue[index].data2   = 0;
    }

    eventQueueHeadPtr = eventQueue;
    eventQueueTailPtr = eventQueue;
}


void sendEvent(event_t eventId, int32_t data1, int32_t data2)
{
    eventQueue_t    *nextHeadPtr = eventQueueHeadPtr;

    if (eventId == timerEvt_cancelTimer)
    {
        /*  This event is special.  To avoid some possible race conditions, we should
            immediately process this event to cancel any queued events and any active
            timers with this event ID.
        */
        int16_t index;

        for (index = 0; index < MAX_EVENTS; index++)
        {
            if (eventQueue[index].eventId == data1)
            {
                eventQueue[index].eventId = systemEvt_nullEvt;
            }
        }
        cancelTimer((event_t)data1);
        return;
    }

    //  Find the next potentially empty slot and confirm it can be used
    nextHeadPtr++;
    if (nextHeadPtr >= &eventQueue[MAX_EVENTS])
    {
        nextHeadPtr = eventQueue;
    }

    if (nextHeadPtr != eventQueueTailPtr)
    {
        //  Save new event parameters in the queue
        eventQueueHeadPtr->eventId = eventId;
        eventQueueHeadPtr->data1   = data1;
        eventQueueHeadPtr->data2   = data2;

        //  Update the official pointer to log this new event
        eventQueueHeadPtr = nextHeadPtr;
    }
    else
    {
        //  The event queue is full
        dbgSerial.println(F("Event queue overflow"));
        showAllQueuedEvents();
    }
}


void cancelQueuedTimerStartEvents(event_t eventId)
{
    for (int index = 0; index < MAX_EVENTS; index++)
    {
        eventQueue_t    *eventQueuePtr = &eventQueue[index];
        if ((eventQueuePtr->eventId == timerEvt_startTimer  ||  eventQueuePtr->eventId == timerEvt_startPeriodicTimer)  &&  eventQueuePtr->data1 == eventId)
        {
            eventQueue[index].eventId = systemEvt_nullEvt;
        }
    }
}


void processEvents(void)
{
    updateTimerEvents();

#ifndef _WIN32
//  dbgSerial.println(F("EventLoopTop"));
#endif // !_WIN32

    while (eventQueueHeadPtr != eventQueueTailPtr)
    {
        digitalWrite(13, HIGH); //  Only set this GPIO when events are queued

//#define SHOW_EACH_EVENT
#ifdef  SHOW_EACH_EVENT
        dbgSerial.print(F("RepEvt: "));
        dbgSerial.print(eventQueueTailPtr->eventId, HEX);
#ifdef  NotUsed
        dbgSerial.print(F(" "));
        dbgSerial.print(eventQueueTailPtr->data1, HEX);
        dbgSerial.print(F(" "));
        dbgSerial.println(eventQueueTailPtr->data2, HEX);
#else   //  NotUsed
        dbgSerial.println();
#endif  //  NotUsed

//      for (volatile long index = 0; index < 60000; index++);
#endif

        switch (EVT2OBJ(eventQueueTailPtr->eventId))
        {
        case evtObj_Timer:         timerObj_EventHandler            (eventQueueTailPtr); break;
        case evtObj_MsmtMgr:       msmtMgrObj_EventHandler          (eventQueueTailPtr); break;
        case evtObj_TallyTracker:  tallyTrkObj_EventHandler         (eventQueueTailPtr); break;
        case evtObj_Binning:       binningObj_EventHandler          (eventQueueTailPtr); break;
#ifndef _WIN32
        case evtObj_LoadCell:      loadCellObj_EventHandler         (eventQueueTailPtr); break;
        case evtObj_SerialRead:    serialReadObj_EventHandler       (eventQueueTailPtr); break;
        case evtObj_AdcCvt:        adcCvtObj_EventHandler           (eventQueueTailPtr); break;
        case evtObj_Accumulation:  accumulationObj_EventHandler     (eventQueueTailPtr); break;
#endif // !_WIN32
        default:
            if (eventQueueTailPtr->eventId != systemEvt_nullEvt)
            {
                //  This is an error, some unrecognized event was sent.
            }
            break;
        }

        //  Remove this event from the queue
        eventQueueTailPtr++;
        if (eventQueueTailPtr >= &eventQueue[MAX_EVENTS])
        {
            eventQueueTailPtr = eventQueue;
        }
    }

#ifdef  SHOW_EACH_EVENT
//  dbgSerial.print(F("emptyQueue\n"));
#endif  //  SHOW_EACH_EVENT

    digitalWrite(13, LOW);

//  dbgSerial.println(F("EventLoopBottom"));

//  showAllTimers();
}


void initializeEvents(void)
{
    pinMode(13, OUTPUT);        //  Used to show loading

    nextionSerial.begin(9600);
    nextionSerial.print(F("baud=38400"));
    nextionSerial.print(F("\xFF\xFF\xFF"));
    delay(20);
    nextionSerial.print(F("baud=38400"));
    nextionSerial.print(F("\xFF\xFF\xFF"));
    delay(20);

    nextionSerial.begin(38400);
    nextionSerial.print(F("talk to nextion"));

    dbgSerial.begin(115200);

    initEventQueue();
    dbgSerial.println(F("Init stage #1"));
    msmtMgrObj_Initialize();

    dbgSerial.println(F("Init stage #2"));
    tallyTrkObj_Initialize();

#ifndef _WIN32
    accumulationObj_Initialize();

    adcCvtObj_Initialize();
    dbgSerial.println(F("Init done, #3"));
    loadCellObj_Initialize();
    dbgSerial.println(F("Init stage #4"));
    serialReadObj_Initialize();
    dbgSerial.println(F("Init stage #5"));
    binningObj_Initialize();
    dbgSerial.println(F("Init stage #6 -- Init done"));

#endif // !_WIN32
}
