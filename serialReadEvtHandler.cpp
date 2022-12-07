
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <arduino.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include "commonHeader.h"
#include "win32shims.h"
#include "events.h"
#include "serialReadEvtHandler.h"
#include "calibration.h"
#include "msmtMgrEvtHandler.h"
#include "accumulationMgr.h"


#define     MAX_READ_BUFFER_BYTES       25

static      uint8_t readDataBuff[2 * MAX_READ_BUFFER_BYTES],
                    *readDataHeadPtr,
                    *readDataTailPtr;

static event_t parseNewCommand(uint8_t *cmdPtr);

extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;

static const char PROGMEM    cmdString_Ping[] PROGMEM = "ping";
static const char PROGMEM    cmdString_Tare[] PROGMEM = "tare";
static const char PROGMEM    cmdString_Ice[]  PROGMEM = "ice";
static const char PROGMEM    cmdString_Cook[] PROGMEM = "cook";
static const char PROGMEM    cmdString_Raw[]  PROGMEM = "raw,";
static const char PROGMEM    cmdString_Rate[] PROGMEM = "rate,";
static const char PROGMEM    cmdString_Cclr[] PROGMEM = "cclr,";
static const char PROGMEM    cmdString_Cset[] PROGMEM = "cset,";
static const char PROGMEM    cmdString_Gcal[] PROGMEM = "gcal,";
static const char PROGMEM    cmdString_Noise[]           PROGMEM = "noise,";
static const char PROGMEM    cmdString_Graph[]           PROGMEM = "graph,";
static const char PROGMEM    cmdString_Quant[]           PROGMEM = "quant,";
static const char PROGMEM    cmdString_FinalMeas[]       PROGMEM = "final,";
static const char PROGMEM    cmdString_FastFwdScreen[]   PROGMEM = "ff";
static const char PROGMEM    cmdString_ToggleLogState[]  PROGMEM = "tlog";      //  Switch debug logging on/off
static const char PROGMEM    cmdString_ToggleTareState[] PROGMEM = "tstate";    //  Switch debug on/off for main screen
static const char PROGMEM    cmdString_VersionReqState[] PROGMEM = "vreq";      //  ask for the current version string
static const char PROGMEM    cmdString_TallyStatus[] PROGMEM = "tallyStatus,";  //  init a new random key
static const char PROGMEM    cmdString_TallyValid[] PROGMEM = "tallyValid,";  //  We got a new validation response
static const char PROGMEM    cmdString_AccumScreenEnable[]  PROGMEM = "showAccum";  //  Show accumulation screen
static const char PROGMEM    cmdString_AccumScreenDisable[] PROGMEM = "hideAccum";  //  Hide accumulation screen


static event_t parseNewCommand(uint8_t *cmdPtr)
{
    event_t                 returnValue = systemEvt_nullEvt;

    if (0)
    {
        dbgSerial.print(F("IncomingStrLen="));
        dbgSerial.print(strlen((char *)cmdPtr));
        dbgSerial.print(F("  "));
        dbgSerial.println((char *)cmdPtr);
    }

         if (                 *cmdPtr                 == '\0')       returnValue = serialReadEvt_NullString;
    else if (strncmp_P((char *)cmdPtr, cmdString_Ping,      4) == 0) returnValue = serialReadEvt_Ping;
    else if (strncmp_P((char *)cmdPtr, cmdString_Tare,      4) == 0) returnValue = serialReadEvt_SetTare;
    else if (strncmp_P((char *)cmdPtr, cmdString_Ice,       3) == 0) returnValue = serialReadEvt_IceCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Cook,      4) == 0) returnValue = serialReadEvt_CookCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Raw,       4) == 0) returnValue = serialReadEvt_RawCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Rate,      5) == 0) returnValue = serialReadEvt_RateCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Cclr,      5) == 0) returnValue = serialReadEvt_CclrCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Cset,      5) == 0) returnValue = serialReadEvt_CsetCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Gcal,      5) == 0) returnValue = serialReadEvt_GcalCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_FinalMeas, 6) == 0) returnValue = serialReadEvt_FinalCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Noise,     6) == 0) returnValue = serialReadEvt_NoiseCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Graph,     6) == 0) returnValue = serialReadEvt_GraphCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_Quant,     6) == 0) returnValue = serialReadEvt_QuantCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_FastFwdScreen,   2) == 0) returnValue = serialReadEvt_FastFwdCmd;
    else if (strncmp_P((char *)cmdPtr, cmdString_ToggleTareState, 6) == 0) returnValue = serialReadEvt_ToggleTareDebugState;
    else if (strncmp_P((char *)cmdPtr, cmdString_ToggleLogState,  4) == 0) returnValue = serialReadEvt_ToggleLogDebugState;
    else if (strncmp_P((char *)cmdPtr, cmdString_VersionReqState, 4) == 0) returnValue = serialReadEvt_RequestVersionInfo;
    else if (strncmp_P((char *)cmdPtr, cmdString_AccumScreenEnable,  9) == 0) returnValue = serialReadEvt_AccumScreenEnable;
    else if (strncmp_P((char *)cmdPtr, cmdString_AccumScreenDisable, 9) == 0) returnValue = serialReadEvt_AccumScreenDisable;

    if (returnValue == serialReadEvt_FastFwdCmd)
    {
        dbgSerial.println(F("parsed FastFwd"));
    }
    if (returnValue != systemEvt_nullEvt  &&  returnValue != serialReadEvt_NullString)
    {
        //  Halt the flush timer if we just parsed a command
        sendEvent(timerEvt_cancelTimer, serialReadEvt_CmdFlushTimeout, 0);
        if (readDataHeadPtr != readDataTailPtr)
        {   //  Restart the flush timer if there is still more stuff in the serial FIFO buffer
            sendEvent(timerEvt_startTimer, serialReadEvt_CmdFlushTimeout, 500);
        }
        else
        {
            //  Reset the serial fifo to make debug easier
            readDataHeadPtr = readDataBuff;
            readDataTailPtr = readDataBuff;
        }

        if (0)
        {
            dbgSerial.println(F("Some match found"));
        }
    }

    return returnValue;
}


void serialReadObj_Initialize(void)
{
    readDataHeadPtr = readDataBuff;
    readDataTailPtr = readDataBuff;

    sendEvent(timerEvt_startPeriodicTimer, serialReadEvt_CheckBoardSignatureTimeout, 4321);   //  Random timeout period

    /*  Serial object is already initialized, so just let it stay  */
    nextionSerial.setTimeout(1);
}


void serialReadObj_EventHandler(eventQueue_t* event)
{
    bool    done,
            completeResponseAvailable = false;

    int16_t index,
            readDataCnt,
            readData;

    uint8_t *newHeadPtr;

    switch (event->eventId)
    {
    case serialReadEvt_DataAvailable:
        /*  serial data has arrived  */

        if  (0)
        {
            dbgSerial.println(F("ReadEvtDataAvail"));
        }
        done = false;
        while (!done)
        {
            while (nextionSerial.available())
            {
                readDataCnt = nextionSerial.available();
                for (index = 0; index < readDataCnt; index++)
                {
                    readData = nextionSerial.read();

                    if (0)
                    {
                        dbgSerial.print(F("NextChar="));
                        dbgSerial.print(readData, HEX);
                        dbgSerial.print(F("  "));
                        dbgSerial.println((char)readData);
                    }

                    //  Ignore all the characters in the auto-responses
                    if ((readData != -1)  &&  (readData != '\x1A')  &&  (readData != '\x12')  &&  (readData != 0xFF))
                    {

                        if (readDataHeadPtr == readDataTailPtr)
                        {   //  Fifo was empty and now not empty, start a flush timeout to avoid stalls
                            sendEvent(timerEvt_cancelTimer, serialReadEvt_CmdFlushTimeout,   0);
                            sendEvent(timerEvt_startTimer,  serialReadEvt_CmdFlushTimeout, 500);
                        }

                        //  Put the new character in the read fifo
                        newHeadPtr = readDataHeadPtr + 1;
                        if (newHeadPtr >= &readDataBuff[MAX_READ_BUFFER_BYTES])
                        {
                            newHeadPtr = readDataBuff;
                        }

                        if (newHeadPtr != readDataTailPtr)
                        {
                            //  Skip storing carriage returns, turn newlines into nulls
                            if (readData == '\n'  ||  readData == '\r')
                            {
                                readData = '\0';
                                completeResponseAvailable = true;
//                              dbgSerial.print(F("Response Available\n"));
                            }
                            readDataHeadPtr[0]                     = (uint8_t)readData;
                            readDataHeadPtr[MAX_READ_BUFFER_BYTES] = (uint8_t)readData;

                            if (0)
                            {
                                dbgSerial.print(F(">"));
                                dbgSerial.print((char)readData);
                                dbgSerial.println(F("<"));
                            }

                            readDataHeadPtr = newHeadPtr;
                            if (completeResponseAvailable)
                            {
                                uint8_t *newTailPtr = readDataTailPtr;
                                event_t matchEventId;

                                //  Remove any preceding nulls in the queue
                                while (newHeadPtr != newTailPtr  &&  *newTailPtr == '\0')
                                {
                                    newTailPtr++;
                                    if (newTailPtr >= &readDataBuff[MAX_READ_BUFFER_BYTES])
                                    {
                                        newTailPtr = readDataBuff;
                                    }
                                }

                                if (0)
                                {
                                    dbgSerial.print(F("Parsing "));
                                    dbgSerial.print((char *)newTailPtr);
                                    dbgSerial.print(F("  "));
                                    dbgSerial.print((int)newTailPtr[0], HEX);
                                    dbgSerial.print(F(" "));
                                    dbgSerial.print((int)newTailPtr[1], HEX);
                                    dbgSerial.print(F(" "));
                                    dbgSerial.print((int)newTailPtr[2], HEX);
                                    dbgSerial.print(F(" "));
                                    dbgSerial.print((int)newTailPtr[3], HEX);
                                    dbgSerial.print(F("\n"));
                                }

                                bool    validCmdFound = true;

                                switch (parseNewCommand(newTailPtr))
                                {
                                case systemEvt_nullEvt:   //  No match or nothing to do
//                                  dbgSerial.println(F("nullEvt"));
                                    if (strlen(newTailPtr) > 0)
                                    {
                                        String    testStr = newTailPtr;
                                        nextionSerial.print(F("D.nextion.txt=\"Nothing to do >"));
                                        nextionSerial.print(testStr);
                                        nextionSerial.print(F("<\"\xFF\xFF\xFF"));
                                    }
                                    validCmdFound = false;
                                    break;

                                case serialReadEvt_Ping:
//                                  dbgSerial.println(F("cmdPing"));
                                    nextionSerial.print(F("pingTimer.tim=1000\xFF\xFF\xFF"));
                                    break;

                                case serialReadEvt_SetTare:
//                                  dbgSerial.println(F("cmdTare"));
                                    setCurrentTarePoint(sensorType_DistanceWidth,  0);
                                    setCurrentTarePoint(sensorType_DistanceHeight, 0);
                                    setCurrentTarePoint(sensorType_DistanceLength, 0);
                                    setCurrentTarePoint(sensorType_Weight,         0);

                                    setTareMark(sensorType_DistanceWidth);
                                    setTareMark(sensorType_DistanceHeight);
                                    setTareMark(sensorType_DistanceLength);
                                    setTareMark(sensorType_Weight);

                                    enableCalibrationCalcPrinting();
                                    printCalArray(sensorType_DistanceLength);
                                    printCalArray(sensorType_DistanceWidth);
                                    printCalArray(sensorType_DistanceHeight);
                                    printCalArray(sensorType_Weight);
                                    disableCalibrationCalcPrinting();

#ifndef     _WIN32
                                    if (shouldShowCsvForDebug())
                                    {
                                        //  report calibration setting events
                                        dbgSerial.print  (F(":,"));
                                        dbgSerial.print  (5);
                                        dbgSerial.print  (F(","));
                                        dbgSerial.print  ((int32_t)millis());
                                        dbgSerial.println(F(",0,0,0"));

                                    }
#endif  //  _WIN32
                                    break;

                                case serialReadEvt_IceCmd:
//                                  dbgSerial.println(F("cmdIce"));
                                    //  Go cold.  Disable all raw and final measurement reporting.
                                    sendEvent(msmtMgrEvt_DisableRawReports, 0, 0);
                                    sendEvent(msmtMgrEvt_SetReportRate,     0, 0);
                                    break;

                                case serialReadEvt_RawCmd:
//                                  dbgSerial.println(F("cmdRaw"));
                                    {
                                        String  numString = &readDataTailPtr[4];
                                        int16_t sensorId  = numString.toInt();

                                        //  Turn off all raw reports.  No sensor Id needed, sending
                                        //  this event will disable all sensor raw reports.
                                        sendEvent(msmtMgrEvt_DisableRawReports, 0, 0);

                                        if (sensorId >= 0  &&  sensorId <= 4)
                                        {
                                            //  Turn on raw data for sensor SS [0..4]
                                            sendEvent(msmtMgrEvt_EnableRawReports, sensorId, 0);
                                        }
                                    }
                                    break;

                                case serialReadEvt_CookCmd:
//                                  dbgSerial.println(F("cmdCook"));
                                    {
                                        //  Turn off all raw data reports
                                        sendEvent(msmtMgrEvt_DisableRawReports, 0, 0);
                                        sendEvent(msmtMgrEvt_SetReportRate, 1000, 0);   //  Go back to default if we don't have one already
                                    }
                                    break;

                                case serialReadEvt_RateCmd:
//                                  dbgSerial.println(F("cmdRate"));
                                    {
                                        //  Set reporting rate, in milliseconds.  Anything smaller than 100 will
                                        //  be treated as 100 mSec.  Otherwise, sensor reports based on an
                                        //  asynchronous timer

                                        String  numString = &readDataTailPtr[5];
                                        int16_t newRate   = numString.toInt();

//                                      dbgSerial.print(F("NewRate="));
//                                      dbgSerial.println(newRate);

                                        sendEvent(msmtMgrEvt_SetReportRate, newRate, 0);
                                    }
                                    break;

                                case serialReadEvt_CclrCmd:
//                                  dbgSerial.println(F("cmdCclr"));
                                    {
                                        /*  Calibration clear.   Tell sensor to clear a stored calibration data point.
                                         *  Screen may then use CSEND to update the sensor calibration table.  SS is
                                         *  sensor ID [0..4], ii is index [0..9],
                                         *
                                         */
                                        String  tmpString1= &readDataTailPtr[5];
                                        int16_t sensorId  = tmpString1.toInt();

                                        char    *scanPtr = &readDataTailPtr[5];

                                        //  Move up to the second comma separator
                                        while (*scanPtr != ','  &&  *scanPtr != '\0')
                                        {
                                            scanPtr++;
                                        }

                                        if (*scanPtr == ',')
                                        {
                                            //  Skip the comma, move to the next parameter
                                            scanPtr++;
                                        }

                                        String  tmpString2= scanPtr;
                                        int16_t calIndex  = tmpString2.toInt();

#ifdef    DONT_DO
                                        dbgSerial.print(F("\nstring tail="));
                                        dbgSerial.print(scanPtr);

                                        dbgSerial.print(F("   SensorID="));
                                        dbgSerial.print(sensorId);
                                        dbgSerial.print(F("    calIndex="));
                                        dbgSerial.println(calIndex);
                                        dbgSerial.println();
#endif
                                        deleteCalPointWithIndex(sensorId, calIndex);
                                    }
                                    break;

                                case serialReadEvt_CsetCmd:
//                                  dbgSerial.println(F("cmdCset"));
                                    {
                                        String  tmpString1= &readDataTailPtr[5];
                                        int16_t sensorId  = tmpString1.toInt();
                                        int32_t xxValue;
                                        int16_t yyValue;
                                        char    *scanPtr;

                                        //  Find the next comma separating sensor from X
                                        scanPtr = &readDataTailPtr[5];
//                                      dbgSerial.print(F("Starting X string="));
//                                      dbgSerial.println(scanPtr);

                                        while (*scanPtr != ','  &&  *scanPtr != '\0')
                                        {
                                            scanPtr++;
                                        }
                                        if (*scanPtr == ',')
                                        {
                                            scanPtr++;  //  Move to the start of X
                                            String  xxString = scanPtr;
                                            xxValue = xxString.toInt();
                                        }

                                        //  Find the next comma separating X from Y
                                        scanPtr = &readDataTailPtr[7];
                                        while (*scanPtr != ','  &&  *scanPtr != '\0')
                                        {
                                            scanPtr++;
                                        }
                                        if (*scanPtr == ',')
                                        {
                                            scanPtr++;  //  Move to the start of Y
                                            String  tmpString3= scanPtr;
                                            int16_t yyValue  = tmpString3.toInt();

                                            if (shouldShowCsvForDebug())
                                            {
                                                dbgSerial.print(F("adding sensorId="));
                                                dbgSerial.print(sensorId);
                                                dbgSerial.print(F("adding xxValue="));
                                                dbgSerial.print(xxValue);
                                                dbgSerial.print(F("adding yyValue="));
                                                dbgSerial.println(yyValue);
                                            }
                                            addCalPoint(sensorId, (int32_t)xxValue, (int16_t)yyValue);
                                        }
                                    }
                                    break;

                                case serialReadEvt_GcalCmd:
//                                  dbgSerial.println(F("getCalibration"));
                                    {
                                        //  Get calibration.  Report all calibration points for sensor ID [0..4]

                                        String  numString = &readDataTailPtr[5];
                                        int16_t sensorId  = numString.toInt();

                                        /*  Report all sensor values for one sensor ID  */
                                        for (int16_t index = 0; index < MAX_CAL_POINTS_PER_SENSOR; index++)
                                        {
                                            nextionSerial.print(F("D.cal"));
                                            nextionSerial.print(index);
                                            nextionSerial.print(F(".val="));
                                            nextionSerial.print(getCalPoint_Y_byIndex(sensorId, index));
                                            nextionSerial.print(F("\xFF\xFF\xFF"));

                                            if (0)
                                            {
                                                dbgSerial.print(F("D.cal"));
                                                dbgSerial.print(index);
                                                dbgSerial.print(F(".val="));
                                                dbgSerial.print(getCalPoint_Y_byIndex(sensorId, index));
                                                dbgSerial.print(F("\n"));
                                            }
                                        }
                                    }
                                    break;

                                case serialReadEvt_FinalCmd:
//                                  dbgSerial.println(F("getFinal"));
                                    {
                                        //  Turn final measurements on and off.

                                        String  numString  = &readDataTailPtr[6];
                                        int16_t measStatus = numString.toInt();

                                        sendEvent((measStatus == 0) ? msmtMgrEvt_DisableFinalReports : msmtMgrEvt_EnableFinalReports, 0, 0);
                                    }
                                    break;

                                case serialReadEvt_NoiseCmd:
                                    {
                                        String  tmpString1= &readDataTailPtr[6];
                                        int16_t sensorId  = tmpString1.toInt();

                                        if (0)
                                        {
                                            dbgSerial.print(F("Saw Noise cmd   sensorId="));
                                            dbgSerial.println(sensorId);
                                        }

                                        sendEvent(msmtMgrEvt_SetNoiseMonitor, sensorId, 0);
                                    }
                                    break;

                                case serialReadEvt_GraphCmd:
                                    {
                                        /*      graph,-1   : decrease scaling 20%
                                                graph,0    : Center all traces
                                                graph,1    : increase scaling 20%
                                        */
                                        String  tmpString1= &readDataTailPtr[6];
                                        int16_t cmdParam  = tmpString1.toInt();

                                        switch (cmdParam)
                                        {
                                        default:
                                        case  0:
//                                          dbgSerial.println(F("serialRead Recenter"));
                                            sendEvent(msmtMgrEvt_GraphScaleRecenter, 0, 0);
                                            break;

                                        case -1:
//                                          dbgSerial.println(F("serialRead Decrease"));
                                            sendEvent(msmtMgrEvt_GraphScaleDecrease, 0, 0);
                                            break;

                                        case  1:
//                                          dbgSerial.println(F("serialRead Increase"));
                                            sendEvent(msmtMgrEvt_GraphScaleIncrease, 0, 0);
                                            break;
                                        }
                                    }
                                    break;

                                case serialReadEvt_QuantCmd:
                                    {
                                        /*      quant,sensor,delta

                                                sensor: 0 to 3
                                                delta : the amount to add to current quantization.  Could be negative
                                                        Special value of -99 will reset back to absolute value of 100
                                        */
                                        String  tmpString1 = &readDataTailPtr[6];
                                        int16_t sensorId   = tmpString1.toInt();
                                        int32_t quantDelta = 0;
                                        char    *scanPtr;

                                        if (0)
                                        {
                                            dbgSerial.print(F("Saw Quant cmd   sensorId="));
                                            dbgSerial.println(sensorId);
                                        }

                                        //  Find the next comma separating sensor from quantization level
                                        scanPtr = &readDataTailPtr[6];
//                                      dbgSerial.print(F("TailPtr="));
//                                      dbgSerial.println(scanPtr);

                                        while (*scanPtr != ','  &&  *scanPtr != '\0')
                                        {
                                            scanPtr++;
                                        }
                                        if (*scanPtr == ',')
                                        {
                                            scanPtr++;  //  Move to the start of srcIndex
                                            String  srcString = scanPtr;
//                                          dbgSerial.print(F("Scan string="));
//                                          dbgSerial.println(srcString);
                                            quantDelta = srcString.toInt();

                                            sendEvent(msmtMgrEvt_QuantizationDelta, sensorId, quantDelta);

                                            if (0)
                                            {
                                                dbgSerial.print(F("Parsed values   Sensor="));
                                                dbgSerial.print(sensorId);
                                                dbgSerial.print(F("   quantDelta="));
                                                dbgSerial.println(quantDelta);
                                            }
                                        }
                                    }
                                    break;

                                case serialReadEvt_FastFwdCmd:
                                    dbgSerial.print(F("FastFwdCmd"));
//                                  dbgSerial.print(F("                      \n"));
//                                  dbgSerial.print(F("                      \n"));
                                    break;

                                case serialReadEvt_ToggleLogDebugState:
                                    toggleCsvForDebug();
                                    break;

                                case serialReadEvt_ToggleTareDebugState:
                                    toggleTareStateForDebug();
                                    break;

                                case serialReadEvt_RequestVersionInfo:
                                    nextionSerial.print(F("Version.txt=\""));
                                    nextionSerial.print(F(__DATE__));
                                    nextionSerial.print(F("\"\xFF\xFF\xFF"));
                                    break;

                                case serialReadEvt_AccumScreenEnable:
                                    dbgSerial.print  (F("AccumScreenEnable\n"));
                                    accumulationObj_EnableAccumulationScreen();
                                    break;

                                case serialReadEvt_AccumScreenDisable:
                                    dbgSerial.print  (F("AccumScreenDisable\n"));
                                    accumulationObj_DisableAccumulationScreen();
                                    break;

                                case serialReadEvt_TallyStatus:
                                    //  Sent when we enter and exit the TallyStatus screen.  Parameter is 1 upon
                                    //  entry, 0 upon exit.
                                    {
                                        String  numString  = &readDataTailPtr[12];
                                        int16_t measStatus = numString.toInt();

                                        dbgSerial.print(F("Received TallyStatus data1="));
                                        dbgSerial.println(measStatus);
                                        sendEvent((measStatus == 0) ? tallyTrkEvt_TallyStatusExit : tallyTrkEvt_TallyStatusEntry, 0, 0);
                                    }
                                    break;

                                case serialReadEvt_TallyValid:
                                    /*  User entered a validation key
                                    */
                                    {
                                        String    numString     = &readDataTailPtr[11];
                                        uint32_t  validationKey = numString.toInt();

                                        dbgSerial.print(F("Parsed TallyValid keyword, validationKey="));
                                        dbgSerial.print(validationKey);
                                        dbgSerial.print(F("\n\n"));

                                        sendEvent(tallyTrkEvt_TallyValid, validationKey, 0);
                                    }
                                    break;
                                }

                                sendEvent(timerEvt_cancelTimer, serialReadEvt_ClearReceiveBuffer,   0);
                                sendEvent(timerEvt_startTimer,  serialReadEvt_ClearReceiveBuffer, 500);

                                completeResponseAvailable = false;

                                /*  Remove any  non-null old command letters from the buffer  */
                                while (*newTailPtr != '\0'  &&  newTailPtr != readDataHeadPtr)
                                {
                                    if (0)
                                    {
                                        dbgSerial.print(F("Remove : "));
                                        dbgSerial.print((char)*newTailPtr);
                                        dbgSerial.print(F("\n"));
                                    }
                                    newTailPtr++;
                                    if (newTailPtr >= &readDataBuff[MAX_READ_BUFFER_BYTES])
                                    {
                                        newTailPtr = readDataBuff;
                                    }
                                }

                                while (newTailPtr != readDataHeadPtr  &&  *newTailPtr == '\0')
                                {
                                    //  There is still one or more nulls at the front of the queue to remove, so get rid of it
                                    newTailPtr++;
                                    if (newTailPtr >= &readDataBuff[MAX_READ_BUFFER_BYTES])
                                    {
                                        newTailPtr = readDataBuff;
                                    }
                                }
                                readDataTailPtr = newTailPtr;
                            }
                        }
                        else
                        {
                            /*  Buffer overflow, so drop the new character  */
                        }
                    }
                    else
                    {
//                      dbgSerial.print(F("tossed\n"));
                    }
                }
                if (nextionSerial.available() == 0)
                {
                    done = true;
                }
            }
        }
        break;

    case serialReadEvt_CmdFlushTimeout:
        //  Delete anything in the fifo to re-sync it.
        readDataHeadPtr = readDataBuff;
        readDataTailPtr = readDataBuff;
        break;

    case serialReadEvt_ClearReceiveBuffer:
//      dbgSerial.print("CheckingEmpty\n");
        while (nextionSerial.available())
        {   //  Empty the incoming buffer
            nextionSerial.read();
//          dbgSerial.print("empty1\n");
        }
        break;

    case serialReadEvt_SetTare:
        //  Distribute the Tare event to all sensors
        setCurrentTarePoint(sensorType_DistanceWidth,  0);
        setCurrentTarePoint(sensorType_DistanceHeight, 0);
        setCurrentTarePoint(sensorType_DistanceLength, 0);
        setCurrentTarePoint(sensorType_Weight,         0);

        //  Only show a log entry for manual tare events, they are external
        if (shouldShowCsvForDebug())
        {
            //  report setting a new tare point
            dbgSerial.print  (F(":,"));
            dbgSerial.print  (3);
            dbgSerial.print  (F(","));
            dbgSerial.print  (millis());
            dbgSerial.print  (F(","));
            dbgSerial.print  (0);
            dbgSerial.print  (F(","));
            dbgSerial.println(0);
        }
        break;

    case serialReadEvt_CheckBoardSignatureTimeout:
        {
            int     startAddress = EEPROM.length() - 4;

            if (EEPROM.read(startAddress + 0) != 'R'  ||
                EEPROM.read(startAddress + 1) != 'O'  ||
                EEPROM.read(startAddress + 2) != 'S'  ||
                EEPROM.read(startAddress + 3) != 'S')
            {
                nextionSerial.print(F("page Start\xFF\xFF\xFF"));
                nextionSerial.print(F("StartTodo.txt=\"Voltage error.  Contact SinoPro\"\xFF\xFF\xFF"));
/*
                dbgSerial.print(F("Signing key is "));
                dbgSerial.print((char)EEPROM.read(startAddress + 0));
                dbgSerial.print((char)EEPROM.read(startAddress + 1));
                dbgSerial.print((char)EEPROM.read(startAddress + 2));
                dbgSerial.print((char)EEPROM.read(startAddress + 3));
                dbgSerial.print(F("\n"));
*/
            }
            else
            {
//              dbgSerial.print(F("The Mega is valid\n"));
            }
        }
        break;
    }
}


void serialEvent1()
{
    /*  This function is called at the end of loop() when there is serial data
        ready to receive.
    */
    sendEvent(serialReadEvt_DataAvailable, 0, 0);
//  dbgSerial.print("SerialEvent1\n");
}

void serialEvent()
{
    /*  This function is called at the end of loop() when there is serial data
        ready to receive.
    */
    sendEvent(serialReadEvt_DataAvailable, 0, 0);
//  dbgSerial.print("SerialEvent\n");
}
