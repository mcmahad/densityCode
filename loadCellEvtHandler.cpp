

//  #define   MEASURE_CHANNEL_B
#include <arduino.h>

#include "ADS1232.h"
#include "ADS1256.h"

#include "events.h"
#include "calibration.h"

extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;


// ADS1232 circuit wiring
const int16_t LOADCELL_PWDN_PIN_CELL  = 2;
const int16_t LOADCELL_SCK_PIN_CELL   = 3;
const int16_t LOADCELL_DOUT_PIN_CELL  = 4;

#define   LOADCELL_RESTART_TIMEOUT_MSEC    2000

void showSlopeAndIntercept();

ADS1232 scale_cell;


void loadCellObj_Initialize(void)
{
    sendEvent(loadCellEvt_RestartLoadCell, 0, 0);

//  sendEvent(timerEvt_startPeriodicTimer, loadCellEvt_ScreenTestTimeout, 1000);
}


void loadCellObj_EventHandler(eventQueue_t* event)
{
    ADS1232 *scalePtr;

    scalePtr = &scale_cell;

/*
    dbgSerial.print(F("LoadCell Event: "));
    dbgSerial.print(event->eventId, HEX);
    dbgSerial.print(F("  "));
    dbgSerial.print(event->data1, HEX);
    dbgSerial.print(F("  "));
    dbgSerial.println(event->data2, HEX);
*/

    switch (event->eventId)
    {
    case loadCellEvt_ScreenTestTimeout:
        dbgSerial.println(F("screenTestTimeout"));
        {
            static  int   screenNumber;

            screenNumber = (screenNumber == 0) ? 1 : 0;

            switch (screenNumber)
            {
            default:
            case 0:
                nextionSerial.print(F("page Main"));
                nextionSerial.print(F("\xFF\xFF\xFF"));
                break;

            case 1:
                nextionSerial.print(F("page Start"));
                nextionSerial.print(F("\xFF\xFF\xFF"));
                break;
            }
        }
        break;

    case loadCellEvt_RestartLoadCell:
        if (0)
        {
            dbgSerial.println(F("restartLoadCell"));
        }
        /*  This event will re-init the entire load cell.  It sets a timeout for measurement reporting
         *   and will restart the loadcell with this event if there is no measurement.
         *
         */
        scale_cell.begin(LOADCELL_DOUT_PIN_CELL, LOADCELL_SCK_PIN_CELL, LOADCELL_PWDN_PIN_CELL, 0);
        scale_cell.set_FirstDelay(100);
        sendEvent(loadCellEvt_InitializeLoadCell, 0, 0);

        /*  Cancel any timers that may exist  */
        sendEvent(timerEvt_cancelTimer, loadCellEvt_CalibrateAmplifierOffset, 0);
        sendEvent(timerEvt_cancelTimer, loadCellEvt_WaitForLoadCellReady,     0);
        sendEvent(timerEvt_cancelTimer, loadCellEvt_RestartLoadCell,          0);

        /*  Start the measurement report timeout  */
        sendEvent(timerEvt_startTimer, loadCellEvt_RestartLoadCell, LOADCELL_RESTART_TIMEOUT_MSEC);
        break;

    case loadCellEvt_InitializeLoadCell:
        //   Nothing really to do in this event, it could be removed
        if (0
        {
            dbgSerial.println(F("initLoadCell"));
        }
        sendEvent(loadCellEvt_PowerUpLoadCell, 0, 0);
        break;

    case loadCellEvt_PowerUpLoadCell:
        /*  Power up the load cell.  This will bring back the next gain to measure
         *  in the next acquisition.   The cell is assumed to have been powered down
         *  prior to this state.
         *
         *  This will start a conversion.
         */
        if (scalePtr->power_up())
        {
            if (0)
            {
                dbgSerial.println(F("LoadCell power up"));
            }
            /*  Power did turn on, so schedule offset calibration for the amplifier in 600 mSec  */
            sendEvent(timerEvt_startTimer,    loadCellEvt_CalibrateAmplifierOffset, 600);
        }
        else
        {
            if (0)
            {
                dbgSerial.println(F("LoadCell already powered, no calibration needed"));
            }
        }

        /*  The analyzer says this takes XXX mSec and that XXX for delay is the best choice.
         *
         */

        sendEvent(timerEvt_startTimer, loadCellEvt_WaitForLoadCellReady, scalePtr->get_FirstDelay());  //  Internal Osc is 3%, so we need to timeout before 97 mSec
//      showAllTimers();
//      showAllQueuedEvents();
        break;

    case loadCellEvt_WaitForLoadCellReady:
        if (0)
        {
            dbgSerial.println(F("waitForLoadCellReady"));
        }
        if (scalePtr->is_ready())
        {
            scalePtr->set_FirstDelay(scalePtr->get_FirstDelay() - 1);
            sendEvent(loadCellEvt_StartLoadCellCollection, 0, 0);
  //        dbgSerial.println(F("LoadCell is_ready"));
        }
        else
        {
            if (scalePtr->get_FirstDelay() < 104)
            {
              scalePtr->set_FirstDelay(scalePtr->get_FirstDelay() + 1);
            }
            /*  Wait another millisecond and try again  */
            sendEvent(timerEvt_startTimer, loadCellEvt_WaitForLoadCellReady, 1);
        }
        break;

    case loadCellEvt_StartLoadCellCollection:
        if (0)
        {
            dbgSerial.println(F("startLoadCellCollection"));
        }
        /*
         *   Shift 24 bits of data out of the ADC.
         */
        scalePtr->read();
//      dbgSerial.println(F("Report LoadCell"));
//      dbgSerial.println((int32_t)scalePtr->getLastRawValue(), HEX);        // DMMDBG
//      dbgSerial.println(F("send loadcell\n"));
        sendEvent(msmtMgrEvt_ReportRawSensorMsmt, scalePtr->getLastRawValue(), sensorType_Weight);
        sendEvent(timerEvt_cancelTimer, loadCellEvt_RestartLoadCell, 0);
        sendEvent(timerEvt_startTimer,  loadCellEvt_RestartLoadCell, LOADCELL_RESTART_TIMEOUT_MSEC);

        //  Report the raw value we just got.  Only the filtered version, of course
//      dbgSerial.print(F("ReportingRaw SensorId=Weight"));
//      dbgSerial.print(F(" value="));
//      dbgSerial.print((int32_t)scalePtr->getFilteredValue(), HEX);
//      dbgSerial.print(F(" raw="));
//      dbgSerial.println((int32_t)scalePtr->getLastRawValue(), HEX);

        //  Next line is to go back and restart the read loop
        sendEvent(loadCellEvt_PowerUpLoadCell, 0, 0);

//      scalePtr->power_down();
//      sendEvent(timerEvt_startTimer, loadCellEvt_PowerUpLoadCell, 5000);
        break;

    case loadCellEvt_PrintCellWeight:
        dbgSerial.println(F("printCellWeight"));
#ifdef  DONT_DO
        dbgSerial.print(F("Raw="));
        dbgSerial.print(scalePtr->rawMsmt_cnts);
        dbgSerial.print(F("    "));
        dbgSerial.print(scalePtr->rawMsmt_cnts & 0xFFFFFF, HEX);
        dbgSerial.print(F("    "));
        if (scalePtr->scaling_CountsPerGram_x1000 == 0)
        {
            dbgSerial.print(scalePtr->rawMsmt_cnts - scalePtr->tareValue_cnts);
            dbgSerial.print(F(" counts"));
        }
        if (scalePtr->scaling_CountsPerGram_x1000 != 0)
        {
            dbgSerial.print(((int32_t)(scalePtr->rawMsmt_cnts - scalePtr->tareValue_cnts) * 1000L) / scalePtr->scaling_CountsPerGram_x1000);
            dbgSerial.print(F(" grams"));
        }

        dbgSerial.print(F("   Filtered: "));
        dbgSerial.print(scalePtr->getFilteredValue());
        dbgSerial.print(F("  "));

        dbgSerial.print(findYFromCalibration(sensorType_Weight, (int32_t)scalePtr->getFilteredValue()));
        dbgSerial.print(F(" grams"));

        dbgSerial.println(F(""));
#endif
        break;

    case loadCellEvt_SetFilterWeightingPctg:
        dbgSerial.println(F("setFilterWeightingPctg"));
        scalePtr->setFilterWeightPctg(event->data1);
#ifdef  DONT_DO
        dbgSerial.print(F("SetWeighting:"));
        dbgSerial.println(event->data1);
#endif
        break;

    case loadCellEvt_CalibrateAmplifierOffset:
        dbgSerial.println(F("calibrateAmplifierOffset"));
            dbgSerial.println(F("LoadCell calibrateOffset"));
        scalePtr->calibrate_offset();
        break;

    case loadCellEvt_CalibrateWeight:
        /*  data1 : truth weight current on scale, in grams  */
        dbgSerial.println(F("calibrateWeight"));

        scalePtr->set_scale(event->data1);

        //  Save the calibration point
        addCalPoint(sensorType_Weight, scalePtr->getFilteredValue(), event->data1);

#ifdef  DONT_DO
        dbgSerial.print("addCalPoint X=");
        dbgSerial.print(scalePtr->getFilteredValue());
        dbgSerial.print("  Y=");
        dbgSerial.println(event->data1);

        int32_t   testValue;
        testValue = scalePtr->rawMsmt_cnts - scalePtr->tareValue_cnts ;
        dbgSerial.println(testValue);
        testValue = 1000 * (scalePtr->rawMsmt_cnts - scalePtr->tareValue_cnts);
        dbgSerial.println(testValue);
        testValue = (1000 * (scalePtr->rawMsmt_cnts - scalePtr->tareValue_cnts))/ currentWeight_grams;
        dbgSerial.println(testValue);
        dbgSerial.println((testValue == scalePtr->scaling_CountsPerGram_x1000) ? F("Equal") : F("not Equal"));
        dbgSerial.println(sizeof(int16_t));
        dbgSerial.println();
#endif  //  DONT_DO
        break;
    }
}

