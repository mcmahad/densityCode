
#define     _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#ifndef _WIN32
#include <arduino.h>
#else
#include <string>
#include "win32shims.h"
#include "WString.h"
#endif // _WIN32

#include "commonHeader.h"
#include "events.h"
#include "calibration.h"
#include "msmtMgrEvtHandler.h"
#include "graphing.h"
#include "tallyTracker.h"
#include "accumulationMgr.h"

#ifdef  _WIN32
static  HardwareSerial      dbgSerial,
                            nextionSerial;
#else
extern  HardwareSerial      &dbgSerial,
                            &nextionSerial;
#endif  //  _WIN32


//  int32_t getLastFilteredSensorReading(sensorType_t sensor);
void printCalArray(sensorType_t sensor);
void enableCalibrationCalcPrinting(void);
void disableCalibrationCalcPrinting(void);
int getLineCnt(void);


#define MASTER_SCALER       3
#define MAX_DIVISOR_SCALING_LIMIT   (MASTER_SCALER * 900 )
//#define FILTER_INCREASE_COUNT       (MASTER_SCALER *  20 )
#define FILTER_INCREASE_PCTG_LENGTH (75 )
#define FILTER_INCREASE_PCTG_WIDTH  (75 )
#define FILTER_INCREASE_PCTG_HEIGHT (75 )
#define FILTER_INCREASE_PCTG_WEIGHT (75 )

#define FILTER_RELEASE_PCTG_LENGTH (68 )
#define FILTER_RELEASE_PCTG_WIDTH  (68 )
#define FILTER_RELEASE_PCTG_HEIGHT (68 )
#define FILTER_RELEASE_PCTG_WEIGHT (68 )

#define UNASSIGNED_STDEV_START      -999999999
#define POST_SERIAL_PAUSE_MSEC       0

#ifdef  _WIN32
//  #define sensorType_Test     sensorType_DistanceWidth
#define sensorType_Test     sensorType_DistanceLength
#else   //  _WIN32
#define sensorType_Test    100
#endif  //  _WIN32

//  #define sensorType_Test     (sensorType_DistanceWidth)
//  #define sensorType_Test     (sensorType_Weight)

/*  filterScaling_Pctg - A percentage of the contribution for each new
 *   measurement.  A smaller value means each new measurement contributes
 *   less to the filteredSensorReading.
 *
 *   stabilityCount -
 *   1) Increments when a reading is less than or equals 2 grams (or millimeters)
 *      away from the previous measurement.
 *   2) Decrements when a reading is greater than or equals 5 grams (or millimeters)
 *      away from the previous measurement.
 *   3) Limited to a maximum of 100 and a minimum of -10.
 *   4) If stabiltyCount >= 5, the sensor is presumed stable.
 *   5) If the filtered reading is <= 3 grams (or millimeters) and stability count >= 30,
 *      The reading is deemed stable and close to zero so auto-tare occurs. StabilityCount
 *      is forced back to 15 when auto-tare occurs.
 *   6) If all sensors have stability Count >= 5, the measurement is considered stable
 */

typedef int64_t        intx32_t;        //  64-bit signed value, shifted by 32-bits

static int32_t    currentRawSensorReading[sensorType_MaxSensors];
static int32_t    filteredSensorReading  [sensorType_MaxSensors];
static bool       rawSensorReportsEnable [sensorType_MaxSensors];
static int32_t    filterScaling_Pctg     [sensorType_MaxSensors];     //  range = 0..(quantizationLimit-1)
static int8_t     stabilityCount         [sensorType_MaxSensors];
static int32_t    quantizationLimit      [sensorType_MaxSensors];
static int32_t    quantizationRemainder  [sensorType_MaxSensors];
static intx32_t   filteredSensorReading_x32[sensorType_MaxSensors];
static int32_t    sameDirectionNudge     [sensorType_MaxSensors];
static int8_t     showTareMark           [sensorType_MaxSensors];
static bool       showStableMark         [sensorType_MaxSensors];
static int8_t     steadyValueCount       [sensorType_MaxSensors];
static int64_t    lastNumeratorFiltSensor[sensorType_MaxSensors];
static int64_t    lastDenomnatrFiltSensor[sensorType_MaxSensors];
typedef enum
{
    measState_unknown,
    measState_taring,
    measState_transition,
    measState_measuring,
} measuringState_t;



extern float    debugWeight,        //  Global, so they can be printed
                debugHeight,
                debugLength,
                debugWidth;




class tareHistory_t
{
private:
    static    const   int     entryCnt = 20;

    int16_t     itemCnt;
    int32_t     buffer[entryCnt],
        * headPtr,
        * tailPtr,
        sensorIndex;
public:
    tareHistory_t()
    {
        headPtr = buffer;
        tailPtr = buffer;
        memset(buffer, 0, sizeof(buffer));
    }

    void add(int32_t newTare)
    {
        *headPtr = newTare;
        if (++headPtr >= &buffer[entryCnt]) headPtr = buffer;
        itemCnt++;

        if (*headPtr == -3)
        {
            printf("");
        }

        //  Move the tailPtr along
        if (headPtr == tailPtr)
        {
            itemCnt--;
            if (++tailPtr >= &buffer[entryCnt])
            {
                tailPtr = buffer;
            }
        }
    }

    void clear(int32_t newTare)
    {
        headPtr = buffer;
        tailPtr = buffer;
        itemCnt = 0;
        memset(buffer, 0, sizeof(buffer));
        if (newTare != 0) add(newTare);
    }

    int32_t getOldest(void)
    {
        int32_t     returnValue = *tailPtr;

        //  If history is empty, return the current filtered value
        if (headPtr == tailPtr) returnValue = filteredSensorReading[sensorIndex];

        if (returnValue == -36)
        {
            printf("");
        }

        return returnValue;
    }

    int32_t getItemCount(void)
    {
        return itemCnt;
    }

    void setSensorIndex(int32_t thisIndex)
    {
        sensorIndex = thisIndex;
    }
};

int32_t lastReportedRawValue,
        lastReportedFilteredValue;

void setReportedRawAndFilteredValues(int32_t reportedRawValue, uint32_t reportedFilteredValue)
{
    lastReportedRawValue      = reportedRawValue;
    lastReportedFilteredValue = reportedFilteredValue;

    if (lastReportedRawValue > -11200)
    {
        printf("");
    }
}


tareHistory_t           tareHistory           [sensorType_MaxSensors];

static    measuringState_t  taringState              [sensorType_MaxSensors],
                            lastDisplayedTaringState [sensorType_MaxSensors];

static    uint8_t       newValueIsIdenticalCnt[sensorType_MaxSensors],
                        newValueBeyond10Cnt[sensorType_MaxSensors],
                        newValueBelow10Cnt[sensorType_MaxSensors];

static  int32_t         measStateGain         [sensorType_MaxSensors],
                        measReportCntInThisState[sensorType_MaxSensors];      // Count of measurements during current taring state


/*  Info for collecting standard deviation and noise graphing info
*/
static  int8_t      noiseRptActiveSensor = -1;    //  -1 when inactive, 0 to MaxSensors when active

static  int8_t      graphingScaleIndex;     //  Rotates 0 to 2 up and down to determine next scale factor

static  int32_t     rawStdevCnt            = 0,
                    rawStdDevBias          = UNASSIGNED_STDEV_START,
                    rawSquaredStdevSum     = 0,
                    rawStdevSum            = 0,

                    filtStdevCnt           = 0,
                    filtStdDevBias         = UNASSIGNED_STDEV_START,
                    filtSquaredStdevSum    = 0,
                    filtStdevSum           = 0;

static intx32_t   filtX32StdevBias         = 0;

static  float   rawSquaredStdevAccumulator,
                filtSquaredStdevAccumulator,
                rawStdevAccumulator,
                filtStdevAccumulator,
                rawStdevAccumCnt,
                filtStdevAccumCnt;

//  This is the static variable that controls how often stability bar is updated.
static  uint8_t     lastFilterScalingReported = 0;

static  int16_t   lastDisplayedValue[sensorType_MaxSensors];
static  int16_t   lastW,
                  lastH,
                  lastL,
                  lastWt;

static  bool      finalMeasReportsEnabled = false;
static  char      oldDensityString[30];

#ifdef _WIN32
static  bool      showMeasNotRawCsv = false;    //  Used only for Win32 printing control
#endif // _WIN32



void enableCalibrationCalcPrinting(void);
void disableCalibrationCalcPrinting(void);

class shortNoiseEstimater
{
/*  FillPtr points to the oldest entry in the fifo.  It advances once
    _before_ using it to add a new value.

    entries in the history buffer are 24-bit values.

    The sumOfSquares must hold 54-bit values (48-bits, 40 values)

    The sumOfEntries must hold 30-bit values (24-bits, 40 values)
*/
public:
static  const   int     shortHistoryDepth = 8;
static  const   int     historyDepth = 40;

private:
                int32_t historyBuffer[historyDepth],
                        *historyFillPtr;
                int64_t sumOfSquares;
                int32_t sumOfEntries;
                int16_t countOfEntries;     //  Range: [0..historyDepth]

                int64_t shortSumOfSquares;
                int32_t shortSumOfEntries;
                int16_t shortCountOfEntries;     //  Range: [0..shortHistoryDepth]
                int16_t mySensorIndex;
public:
            shortNoiseEstimater();
    void    addNewValue(int32_t newValue);
    void    resetNoiseEstimate(void);
    void    setSensorId(int16_t myNewSensorIndex);
    int32_t getNoiseEstimate(void);
    int32_t getStdDeviation_x100(void);

    int32_t getShortNoiseEstimate(void);
    int32_t getShortStdDeviation_x100(void);
    int32_t getShortRecentAverage(void);

    int32_t getRecentAverage(void);
    int32_t currentSampleCount(void); //  goes up to >20
    void    trimOldIfNeeded(void);
    void    noiseTest(void);
};

int myPrivatePrinter(const char *format, ... )
{
    //  Ignore all, this is a stub
    return 0;
}

shortNoiseEstimater::shortNoiseEstimater()
{
    sumOfSquares   = 0;
    sumOfEntries   = 0;
    countOfEntries = 0;
    historyFillPtr = historyBuffer;
    memset(historyBuffer, 0, sizeof(historyBuffer));

    shortSumOfSquares   = 0;
    shortSumOfEntries   = 0;
    shortCountOfEntries = 0;
}


void shortNoiseEstimater::addNewValue(int32_t newValue)
{
    int64_t     newValue_64b = (int64_t)newValue,
                oldValue_64b = (int64_t)*historyFillPtr,
                newValueSquared_64b = newValue_64b * newValue_64b,
                oldValueSquared_64b = oldValue_64b * oldValue_64b;

    if (countOfEntries >= historyDepth)
    {
        //  Fifo is full, take the old value out subtract whatever the
        //  pointer value is at and then decrement the pointers.

        sumOfSquares -= oldValueSquared_64b;
        sumOfEntries -= (int32_t)oldValue_64b;
    }
    else
    {
        countOfEntries++;
    }
    sumOfSquares = sumOfSquares + newValueSquared_64b;
    sumOfEntries = sumOfEntries + newValue;

    if (0  &&  sensorType_Test == mySensorIndex)
    {
        dbgSerial.print(F("addNewValue() : "));
        dbgSerial.print(countOfEntries);
        dbgSerial.print(F(" "));
        dbgSerial.print(newValue);
        dbgSerial.print(F(" "));
        dbgSerial.print(sumOfEntries);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfSquares);
    }

    /*  This is keeps a std dev for a shorter history.
        If the short and long std dev histories are both
        individually low but the complete deviation is
        large, then we have found a step change and can
        delete the oldest history while keeping some of the
        newest
    */
    if (shortCountOfEntries >= shortHistoryDepth)
    {
        int      oldShortIndex   = shortCountOfEntries;     //  Never >= shortHistoryDepth
        int32_t *ShortRemovalPtr = historyFillPtr - shortHistoryDepth;

        if (ShortRemovalPtr < historyBuffer) ShortRemovalPtr += historyDepth;   //  Gotta add the size of the full buffer

        //  Figure out what the old values are from the history buffer
        oldValue_64b        = (int64_t)*ShortRemovalPtr,
        oldValueSquared_64b = oldValue_64b * oldValue_64b;

        //  Remove the old values
        shortSumOfSquares -= oldValueSquared_64b;
        shortSumOfEntries -= (int32_t)oldValue_64b;
    }
    else
    {
        shortCountOfEntries++;
    }
    shortSumOfSquares += newValueSquared_64b;
    shortSumOfEntries += newValue;

    *historyFillPtr = (int32_t)newValue_64b;
    if (++historyFillPtr >= &historyBuffer[historyDepth])
    {   //  Wrap the history FIFO pointer
        historyFillPtr = historyBuffer;
    }

    if (0  &&  sensorType_Test == mySensorIndex)
    {
        dbgSerial.print(F(" "));
        dbgSerial.print(getNoiseEstimate());
        dbgSerial.println();
    }
}


void shortNoiseEstimater::trimOldIfNeeded(void)
{
    /*  This function tries to trim old noise data if it looks
        like we changed the measurement.

        It does this by looking at recent history, old history,
        and combined recent+old history.

        1) compute recent history stdDev
        2) compute old history stdDev
        3) Compute complete history stdDev
        4)  Compute the ratio of complete / (old + recent)
        5)  If the ratio is 'really big', then hack the old history to adapt faster
    */
    return;

    if (countOfEntries <= shortHistoryDepth * 2)
    {
        //  Not enough data, just return
        return;
    }

    int32_t     completeNoiseEstimate = getNoiseEstimate();     //  Variance of complete amount
    int32_t     oldNoiseEstimate;
    int32_t     recentNoiseEstimate;

    //  Compute old noise estimate
    {
        int64_t     tmpCount        = (int64_t)countOfEntries - shortHistoryDepth,
                    tmpSumOfSquares = (int64_t)sumOfSquares   - shortSumOfSquares,
                    tmpSum          = (int64_t)sumOfEntries   - shortSumOfEntries;

        oldNoiseEstimate = (int32_t)((tmpCount * tmpSumOfSquares - (tmpSum * tmpSum)) / (tmpCount * (tmpCount - 1)));
    }

    //  Compute new noise estimate
    {
        int64_t     tmpCount        = (int64_t)countOfEntries - shortHistoryDepth,
                    tmpSumOfSquares = (int64_t)sumOfSquares   - shortSumOfSquares,
                    tmpSum          = (int64_t)sumOfEntries   - shortSumOfEntries;

        recentNoiseEstimate = (int32_t)(((int64_t)shortCountOfEntries * shortSumOfSquares - (int64_t)(shortSumOfEntries * shortSumOfEntries)) / (shortCountOfEntries * (shortCountOfEntries - 1)));
    }

    //  Keep in mind, each of these noise terms is squared
    if (completeNoiseEstimate / (oldNoiseEstimate * recentNoiseEstimate) > 25)
    {
        int32_t *historyEmptyPtr = historyFillPtr - countOfEntries,
                tmpValue,
                tmpSquare;


        dbgSerial.println(F("Removing old history"));

        if (historyEmptyPtr < historyBuffer)
        {
            historyEmptyPtr += historyDepth;
        }

        //  Flush old history
        while (sumOfEntries > shortHistoryDepth)
        {
            tmpValue  =  (int64_t)*historyEmptyPtr;
            tmpSquare =  tmpValue * tmpValue;

            sumOfSquares -= tmpSquare;
            sumOfEntries -= tmpValue;
            countOfEntries--;

            //  No need to decrement recent history, it won't be wiped

            //  Move the pointer along
            historyEmptyPtr++;
            if (historyEmptyPtr >= &historyBuffer[historyDepth])
            {
                historyEmptyPtr = historyBuffer;
            }
        }
    }
}


void shortNoiseEstimater::resetNoiseEstimate(void)
{
    sumOfSquares        = 0;
    sumOfEntries        = 0;
    countOfEntries      = 0;
    shortSumOfSquares   = 0;
    shortSumOfEntries   = 0;
    shortCountOfEntries = 0;
    historyFillPtr = historyBuffer;
    memset(historyBuffer, 0, sizeof(historyBuffer));        //  Not needed, cosmetic for debug only
    if (1  &&  mySensorIndex == sensorType_Test)
    {
        dbgSerial.println(F("resetNoiseEstimate()"));
    }
}


void shortNoiseEstimater::setSensorId(int16_t myNewSensorIndex)
{
    mySensorIndex = myNewSensorIndex;
}


int32_t shortNoiseEstimater::currentSampleCount(void)
{
    return countOfEntries;
}


int32_t shortNoiseEstimater::getNoiseEstimate(void)
{
    /*  This function returns the variance (stdDev^2)
    */
    int64_t     returnValue_64b,
                sumOfEntries_64b = (uint64_t)sumOfEntries;

    int32_t     returnValue;

    if (0)
    {
        int32_t* mostRecentValPtr = historyFillPtr;

        mostRecentValPtr--;
        if (mostRecentValPtr < historyBuffer)
        {
            mostRecentValPtr += historyDepth;
        }

        dbgSerial.print(F("GetNoiseEst() : "));
        dbgSerial.print((int32_t)*mostRecentValPtr);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)countOfEntries);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfEntries_64b);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)(sumOfSquares >> 32), HEX);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfSquares, HEX);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfSquares);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfSquares, 2);
        dbgSerial.println();
    }

    if (countOfEntries < shortHistoryDepth)
    {   //  Not enough samples, just return some large value
//      dbgSerial.print(F("getNoiseEst() Not enough entries - "));
//      dbgSerial.print(countOfEntries);
//      dbgSerial.println();

        return 1000000001;
    }


    returnValue_64b  = (int64_t)countOfEntries * sumOfSquares - (int64_t)(sumOfEntries_64b * sumOfEntries_64b);
    returnValue_64b += (countOfEntries * (countOfEntries - 1)) / 2;    //  Round up here, before dividing
    returnValue_64b /= (int64_t)countOfEntries;
    returnValue_64b /= (int64_t)countOfEntries - 1;

    if (returnValue_64b > 1000000000)
    {   //  The value won't fit in a 32-bit return value range, so just give back something huge
        return 1000000002;
    }

    returnValue = (int32_t)returnValue_64b;
    if (returnValue >= 10000  ||  returnValue < 0  ||  sumOfSquares < 0)
    {
//      dbgSerial.print(F("Noise Computed too big "));
//      dbgSerial.print(returnValue);
//      dbgSerial.println();
    }
    else
    {
//      dbgSerial.print(F("Noise Computed fine "));
//      dbgSerial.println(returnValue);
    }
    return returnValue;
}


int32_t shortNoiseEstimater::getStdDeviation_x100(void)
{
    int64_t     noiseEstimate = getNoiseEstimate();

    if (noiseEstimate < 20000000)
    {
        //  Convert scaled variance to stdDev x 100
        return (int32_t)sqrt((double)(noiseEstimate * 10000));
    }
    return 100000002;
}


int32_t shortNoiseEstimater::getRecentAverage(void)
{
    if (countOfEntries >= 1)
    {
        return sumOfEntries / countOfEntries;
    }
    return 0;
}


int32_t shortNoiseEstimater::getShortNoiseEstimate(void)
{
    /*  This function returns the variance (stdDev^2)
    */
    int64_t     returnValue_64b,
                sumOfEntries_64b = (uint64_t)shortSumOfEntries;

    int32_t     returnValue;

    if (0)
    {
        int32_t* mostRecentValPtr = historyFillPtr;

        mostRecentValPtr--;
        if (mostRecentValPtr < historyBuffer)
        {
            mostRecentValPtr += historyDepth;
        }

        dbgSerial.print(F("GetShortNoiseEst() : "));
        dbgSerial.print((int32_t)*mostRecentValPtr);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)shortCountOfEntries);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)sumOfEntries_64b);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)(shortSumOfSquares >> 32), HEX);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)shortSumOfSquares, HEX);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)shortSumOfSquares);
        dbgSerial.print(F(" "));
        dbgSerial.print((int32_t)shortSumOfSquares, 2);
        dbgSerial.println();
    }

    if (shortCountOfEntries < shortHistoryDepth)
    {   //  Not enough samples, just return some large value
//      dbgSerial.print(F("getNoiseEst() Not enough entries - "));
//      dbgSerial.print(countOfEntries);
//      dbgSerial.println();

        return 1000000001;
    }
    returnValue_64b  = (int64_t)shortCountOfEntries * shortSumOfSquares - (int64_t)(sumOfEntries_64b * sumOfEntries_64b);
    returnValue_64b += (shortCountOfEntries * (shortCountOfEntries - 1)) / 2;    //  Round up here, before dividing
    returnValue_64b /= (int64_t)shortCountOfEntries;
    returnValue_64b /= (int64_t)shortCountOfEntries - 1;

    if (returnValue_64b > 1000000000)
    {   //  The value won't fit in a 32-bit return value range, so just give back something huge
        return 1000000002;
    }

    returnValue = (int32_t)returnValue_64b;
    if (returnValue >= 10000  ||  returnValue < 0  ||  shortSumOfSquares < 0)
    {
//      dbgSerial.print(F("Noise Computed too big "));
//      dbgSerial.print(returnValue);
//      dbgSerial.println();
    }
    else
    {
//      dbgSerial.print(F("Noise Computed fine "));
//      dbgSerial.println(returnValue);
    }
    return returnValue;
}


int32_t shortNoiseEstimater::getShortStdDeviation_x100(void)
{
    int64_t     noiseEstimate = getShortNoiseEstimate();

    if (noiseEstimate < 20000000)
    {
        //  Convert scaled variance to stdDev x 100
        return (int32_t)sqrt((double)(noiseEstimate * 10000));
    }
    return 100000002;
}


int32_t shortNoiseEstimater::getShortRecentAverage(void)
{
    if (shortCountOfEntries >= 1)
    {
        return shortSumOfEntries / shortCountOfEntries;
    }
    return 0;
}


void shortNoiseEstimater::noiseTest(void)
{
}


void setTareMark(int16_t sensorIndex)
{
    //  This tare comes manual tare
    if (sensorIndex >= 0  &&  sensorIndex < sensorType_MaxSensors)
    {
        showTareMark[sensorIndex] = 1;
    }
}


//  Set the default from commonHeader.h
static bool pleaseShowCsvForDebug = false;



void toggleCsvForDebug(void)
{
    dbgSerial.print("starting togggleCsvForDebug() entry=");
    dbgSerial.println(pleaseShowCsvForDebug ? F("true") : F("false") );

    if (pleaseShowCsvForDebug)
    {
        pleaseShowCsvForDebug = false;
    }
    else
    {
        pleaseShowCsvForDebug = true;
    }

    dbgSerial.print(F("Toggle showing CSV logging state, now : "));
    dbgSerial.println(pleaseShowCsvForDebug ? F("true") : F("false") );
}


bool shouldShowCsvForDebug(void)
{
    return pleaseShowCsvForDebug;
}

int32_t getFilterReleasePctg(sensorType_t sensor)
{
    int32_t     returnValue = 0;

    switch (sensor)
    {
    case sensorType_DistanceLength: returnValue = FILTER_RELEASE_PCTG_LENGTH; break;
    case sensorType_DistanceWidth:  returnValue = FILTER_RELEASE_PCTG_WIDTH;  break;
    case sensorType_DistanceHeight: returnValue = FILTER_RELEASE_PCTG_HEIGHT; break;
    case sensorType_Weight:         returnValue = FILTER_RELEASE_PCTG_WEIGHT; break;
    default:                        returnValue = FILTER_RELEASE_PCTG_WEIGHT; break;
    }
    return returnValue;
}

int32_t getFilterIncreasePctg(sensorType_t sensor)
{
    int32_t     returnValue = 0;

    switch (sensor)
    {
    case sensorType_DistanceLength: returnValue = FILTER_INCREASE_PCTG_LENGTH; break;
    case sensorType_DistanceWidth:  returnValue = FILTER_INCREASE_PCTG_WIDTH;  break;
    case sensorType_DistanceHeight: returnValue = FILTER_INCREASE_PCTG_HEIGHT; break;
    case sensorType_Weight:         returnValue = FILTER_INCREASE_PCTG_WEIGHT; break;
    default:                        returnValue = FILTER_INCREASE_PCTG_WEIGHT; break;
    }
    return returnValue;
}


#ifdef _WIN32
void showMeasCsv_NotRaw(void)
{
    showMeasNotRawCsv = true;
}
#endif  //  _WIN32


static bool pleaseShowTareStateForDebug = false;

static int32_t getMaxDivisorScalingLimit(sensorType_t sensor)
{
    int32_t returnValue;

    //  Returns the max limit based on sensor type
    switch (sensor)
    {
    case sensorType_DistanceLength: returnValue = MAX_DIVISOR_SCALING_LIMIT / 10; break;
    case sensorType_DistanceWidth:  returnValue = MAX_DIVISOR_SCALING_LIMIT / 10; break;
    case sensorType_DistanceHeight: returnValue = MAX_DIVISOR_SCALING_LIMIT / 10; break;
    case sensorType_Weight:         returnValue = MAX_DIVISOR_SCALING_LIMIT /  3; break;
    default:                        returnValue = MAX_DIVISOR_SCALING_LIMIT /  1; break;
    }
    return returnValue;
}

void toggleTareStateForDebug(void)
{
    if (pleaseShowTareStateForDebug)
    {
        pleaseShowTareStateForDebug = false;
    }
    else
    {
        pleaseShowTareStateForDebug = true;
    }
    dbgSerial.print(F("Toggle showing tare state, now : "));
    dbgSerial.println(pleaseShowTareStateForDebug ? F("true") : F("false") );
}


bool shouldShowTareStateForDebug(void)
{
    return pleaseShowTareStateForDebug;
}


static bool shouldUseGainFloorAfterStateChange(sensorType_t sensor)
{
    /*
    *           NOTE : Early return
    */
    return false;

    switch (sensor)
    {
    case sensorType_DistanceLength:
    case sensorType_DistanceWidth:
    case sensorType_DistanceHeight:
        if (measReportCntInThisState[sensor] <= 20)
        {   //  distance sensors sample 20 times/sec
            return true;
        }
        break;

    case sensorType_Weight:
        if (measReportCntInThisState[sensor] <= 10)
        {   //  distance sensors sample 10 times/sec
            return true;
        }
        break;

    default:
        break;
    }
    return false;
}


float   totalDensity_kgPerMeter3 = (float)-1.234,
        densityBiasValue = 0.0;

float   snapshotOldDensity,
        snapshotNewDensity;

//  Create a noise estimator for recent raw sample values
static  shortNoiseEstimater   rawSampleNoiseEstimate[sensorType_MaxSensors];


void msmtMgrObj_Initialize(void)
{
//  dbgSerial.println(F("measInit start"));
//  for (int myIndex = 0; myIndex < 80000; myIndex++) _NOP();

    noiseRptActiveSensor = -1;        //  -1 when inactive, 0 to MaxSensors when active

    pinMode(5, OUTPUT);   //  Make pin 5 a debug toggle pin

//  sendEvent(timerEvt_startPeriodicTimer, msmtMgrEvt_ReportPing, 2000);
    sendEvent(timerEvt_startPeriodicTimer, msmtMgrEvt_PeriodicReportTimeout, 110);  //  Just a bit slower than the slowest sensor update rate of 100 mSec

    for (int16_t index = 0; index < sensorType_MaxSensors; index++)
    {
//      dbgSerial.print  (F("measInit"));
//      dbgSerial.println(index);
//      for (int myIndex = 0; myIndex < 30000; myIndex++) _NOP();

        quantizationLimit [index] = getMaxDivisorScalingLimit(index);        //  Starting default value, adjust if needed
        filterScaling_Pctg[index] = quantizationLimit[index] - 1;

//      dbgSerial.print (F("meas #1"));
//      for (int myIndex = 0; myIndex < 30000; myIndex++) _NOP();

        quantizationRemainder[index] = 0;

//      dbgSerial.println(F("Initialize calls resetNoiseEst()"));
//      for (int myIndex = 0; myIndex < 100000; myIndex++) _NOP();

        rawSampleNoiseEstimate[index].resetNoiseEstimate();
        sameDirectionNudge[index] = 0;
        showTareMark[index] = 0;
        showStableMark[index] = false;

        rawSampleNoiseEstimate[index].setSensorId(index);
        tareHistory[index].setSensorIndex(index);
        measReportCntInThisState[index] = 0;
    }

#ifndef _WIN32
    readCalDataFromFlash(true);
    enableCalibrationCalcPrinting();
    printCalArray(sensorType_DistanceLength);
    printCalArray(sensorType_DistanceWidth);
    printCalArray(sensorType_DistanceHeight);
    printCalArray(sensorType_Weight);
    disableCalibrationCalcPrinting();
#endif // !_WIN32


#ifdef  _WIN32
    if (0)
    {
        const int32_t     testVector[] =
        {
            5614382, 5614384, 5614047, 5614385, 5614387,
            5614404, 5614406, 5614405, 5614397, 5610549,
            5608921, 5608253, 5608259, 5608274, 5608264,
            5608270, 5608276, 5608269, 5609421, 5609605,
            5609035, 5608263, 5610226, 5609054, 5609044,
            5608890, 5609101, 5610395, 5608278, 5608278,
            5609618, 5608287, 5608670, 5609141, 5609618,
            5609429, 5608909, 5608280, 5609060,

            5614382, 5614384, 5614047, 5614385, 5614387,
            5614404, 5614406, 5614405, 5614397, 5610549,
            5608921, 5608253, 5608259, 5608274, 5608264,
            5608270, 5608276, 5608269, 5609421, 5609605,
            5609035, 5608263, 5610226, 5609054, 5609044,
            5608890, 5609101, 5610395, 5608278, 5608278,
            5609618, 5608287, 5608670, 5609141, 5609618,
            5609429, 5608909, 5608280, 5609060
        };
        int32_t     itemCount = 0,
                    stdDevx100,
                    index;

        rawSampleNoiseEstimate[0].resetNoiseEstimate();

        for (index = 0; index < shortNoiseEstimater::shortHistoryDepth - 1; index++)
        {
            rawSampleNoiseEstimate[0].addNewValue(testVector[index]);
            itemCount++;
        }

        index = 0;
        for (index = shortNoiseEstimater::shortHistoryDepth - 1; index < sizeof(testVector)/sizeof(testVector[0]); index++)
        {
            rawSampleNoiseEstimate[0].addNewValue(testVector[index]);
            itemCount++;
            stdDevx100 = rawSampleNoiseEstimate[0].getShortNoiseEstimate();
            printf("%d, %d\n", itemCount, stdDevx100);
            printf("");
        }
    }
#endif  //  _WIN32
}


void msmtMgrObj_EventHandler(eventQueue_t* event)
{
//  dbgSerial.print(F("Event ID=0x"));
//  dbgSerial.println(event->eventId, HEX  );
    static      int16_t     lastNewX;

    switch (event->eventId)
    {
    case msmtMgrEvt_ReportCalibrationSet1:
        /*  data1=sensor
            data2=NewX
        */
        lastNewX = event->data2;
        break;

    case msmtMgrEvt_ReportCalibrationSet2:
        /*  New calibration info was provided, just print something for external playback
            data1=sensor
            data2=NewY
        */
        if (shouldShowCsvForDebug())
        {
            //  report calibration setting events
            dbgSerial.print  (F(":,"));
            dbgSerial.print  (4);
            dbgSerial.print  (F(","));
            dbgSerial.print  ((int32_t)millis());
            dbgSerial.print  (F(","));
            dbgSerial.print  ((int32_t)event->data1);       //  Sensor ID
            dbgSerial.print  (F(","));
            dbgSerial.print  ((int32_t)lastNewX);           //  NewY
            dbgSerial.print  (F(","));
            dbgSerial.println((int32_t)event->data2);       //  NewY
        }
        break;

    case msmtMgrEvt_SetReportRate:
        /*  data1 : new reporting rate, in milliseconds.  0 means disable reports
            data2 : not used
         */
        cancelTimer(msmtMgrEvt_PeriodicReportTimeout);

        if (event->data1 != 0)
        {
            sendEvent(timerEvt_startPeriodicTimer, msmtMgrEvt_PeriodicReportTimeout, event->data1);
        }
        break;


    case msmtMgrEvt_SetNoiseMonitor:
        /*  data1 : sensor ID to use (or -1 to turn off)
            data2 : not used
        */
        noiseRptActiveSensor = event->data1;

        if (0)
        {
            dbgSerial.print(F("SetNoiseMonitor   sensor="));
            dbgSerial.println(noiseRptActiveSensor);
        }

        /*  Mark starting offset to capture it when first sample appears  */
        rawStdevCnt                 = 0;
        rawStdevSum                 = 0,
        rawStdDevBias               = UNASSIGNED_STDEV_START;
        rawSquaredStdevSum          = 0;
        rawSquaredStdevAccumulator  = 0.0;
        rawStdevAccumulator         = 0.0;
        rawStdevAccumCnt            = 0.0;

        filtStdevCnt                = 0;
        filtStdevSum                = 0;
        filtStdDevBias              = UNASSIGNED_STDEV_START;
        filtSquaredStdevSum         = 0;
        filtSquaredStdevAccumulator = 0.0;
        filtStdevAccumulator        = 0.0;
        filtStdevAccumCnt           = 0.0;
        filtX32StdevBias            = 0;

        //  Make this an invalid value to force screen to update this
        lastFilterScalingReported = -100;

        //  Update quantization limit to ensure it paints on the screen
        sendEvent(msmtMgrEvt_QuantizationDelta, event->data1, 0);

        graphStartNewGraph();
        graphingScaleIndex = 0;

        for (int index = 0; index < sensorType_MaxSensors; index++)
        {
            //  Set back to default value
            quantizationLimit[index] = getMaxDivisorScalingLimit(index);

            if (filterScaling_Pctg[index] > quantizationLimit[index] - 1)
            {
                filterScaling_Pctg[index] = quantizationLimit[index] - 1;
            }
        }
        break;

    case msmtMgrEvt_GraphScaleIncrease:
        graphingScaleIndex++;
        if (graphingScaleIndex < 0) graphingScaleIndex = 2;
        if (graphingScaleIndex > 2) graphingScaleIndex = 0;
        {
            int     upScale;
            switch (graphingScaleIndex)
            {
            default:
                graphingScaleIndex = 0;
                upScale = 100;
                break;

            case 0:
            case 1:
                upScale = 100;
                break;
            case 2:
                upScale = 150;
                break;
            }
            graphUpscale(0, upScale);
            graphUpscale(1, upScale);
        }

        if (0)
        {
            int32_t     numerator,
                        myDenominator,
                        offset;

            graphGetScaleAndOffset(&numerator, &myDenominator, &offset);

            nextionSerial.print(F("graphIncrease "));
            nextionSerial.print(numerator);
            nextionSerial.print(F("/"));
            nextionSerial.print(myDenominator);
            nextionSerial.print(F("     offset="));
            nextionSerial.print(offset);
            nextionSerial.print(F("\n"));
        }
        break;

    case msmtMgrEvt_GraphScaleDecrease:
        graphingScaleIndex--;
        if (graphingScaleIndex < 0) graphingScaleIndex = 2;
        if (graphingScaleIndex > 2) graphingScaleIndex = 0;
        {
            int     downScale = 200;
            switch (graphingScaleIndex)
            {
            default:
                graphingScaleIndex = 2;
                break;

            case 2:
            case 0:
                downScale = 200;
                break;

            case 1:
                downScale = 250;
                break;
            }
            graphDownscale(0, downScale);
            graphDownscale(1, downScale);
        }


        if (0)
        {
            int32_t     numerator,
                        myDenominator,
                        offset;

            graphGetScaleAndOffset(&numerator, &myDenominator, &offset);

            nextionSerial.print(F("graphDecrease "));
            nextionSerial.print(numerator);
            nextionSerial.print(F("/"));
            nextionSerial.print(myDenominator);
            nextionSerial.print(F("     offset="));
            nextionSerial.print(offset);
            nextionSerial.print(F("\n"));
        }
        break;

    case msmtMgrEvt_GraphScaleRecenter:
        graphRecenter(0);
        graphRecenter(1);

        if (0)
        {
            int32_t     numerator,
                        myDenominator,
                        offset;

            graphGetScaleAndOffset(&numerator, &myDenominator, &offset);

            dbgSerial.print(F("graphRecenter "));
            dbgSerial.print(numerator);
            dbgSerial.print(F("/"));
            dbgSerial.print(myDenominator);
            dbgSerial.print(F("     offset="));
            dbgSerial.print(offset);
            dbgSerial.print(F("\n"));
        }
        break;

    case msmtMgrEvt_QuantizationDelta:
        /*  data1=sensorId
            data2=newHorizontalSpeed, positive means bigger/faster, negative means smaller/slower, zero means reset
        */
        if (event->data1 >= 0  &&  event->data1 <= sensorType_MaxSensors)
        {
            if      ((int32_t)event->data2 >  0) {graphSetHorizSpeed( 1); }
            else if ((int32_t)event->data2 <  0) {graphSetHorizSpeed(-1); }
            else if ((int32_t)event->data2 == 0) {graphSetHorizSpeed( 0); }
            nextionSerial.print(F("horizSpeed.val="));
            nextionSerial.print(graphGetHorizSpeed());
            nextionSerial.print(F("\xFF\xFF\xFF"));
        }
        break;

    case msmtMgrEvt_MoveToLicenseScreen:
        sendEvent(tallyTrkEvt_CreateNewInventoryKey, 0, 0);
        nextionSerial.print(F("page tallyStatus"));
        nextionSerial.print(F("\xFF\xFF\xFF"));
        break;

    case msmtMgrEvt_EnableFinalReports:
        finalMeasReportsEnabled = true;
        oldDensityString[0]     = '\0';     //  Force an update of all display variables by changing the old copy
        for (int sensorIndex = 0; sensorIndex < sensorType_MaxSensors; sensorIndex++)
        {
            lastDisplayedValue[sensorIndex]++;
        }

        if (tallyTrkObj_GetStickCount() == 0)
        {
            //  No sticks remaining, show the tally screen immediately
            sendEvent(msmtMgrEvt_MoveToLicenseScreen, 0, 0);
        }
        break;

    case msmtMgrEvt_ForceDensityDisplayUpdate:
//      dbgSerial.println(F("Force stick Count update"));
        if (oldDensityString[0] != '\0')
        {
            oldDensityString[0] = '\0';     //  Force an update of all display variables by changing the old copy
        }
        else
        {
            oldDensityString[0] = ' ';     //  Force an update of all display variables by changing the old copy
            oldDensityString[1] = '\0';
        }
        break;

    case msmtMgrEvt_DisableFinalReports:
        finalMeasReportsEnabled = false;
        oldDensityString[0]     = '\0';     //  Force an update of all display variables by changing the old copy
        for (int sensorIndex = 0; sensorIndex < sensorType_MaxSensors; sensorIndex++)
        {
            lastDisplayedValue[sensorIndex]++;
        }
        break;

    case msmtMgrEvt_EnableRawReports:
        /*  data1 : enable raw reports sensor ID [0..4]
         *  data2 : not used
         */
        {
            for (int16_t index = 0; index < sensorType_MaxSensors; index++)
            {
                rawSensorReportsEnable[index] = false;
            }
        }

        if (event->data1 >= 0  &&  event->data1 < sensorType_MaxSensors)
        {
            rawSensorReportsEnable[(int)(event->data1)] = true;

            if (0)
            {
                dbgSerial.print(F("Enabling raw sensor #"));
                dbgSerial.println(event->data1);
            }
        }
        break;

    case msmtMgrEvt_DisableRawReports:
        /*  data1 : not used
         *  data2 : not used
         */
        {
            for (int16_t index = 0; index < sensorType_MaxSensors; index++)
            {
                rawSensorReportsEnable[index] = false;
            }
        }
        break;

    case msmtMgrEvt_PeriodicReportTimeout:
        //  Report all sensors
        {
            String      densityString;    //  Use a string so we can convert float to string
            char        tmpArray[sizeof(oldDensityString)];

static      bool        lastStable   = 0;
static      int32_t     last_DRawVal = 0;   //  last debug screen raw value.  This is 24 bits
static      int8_t      colorIndex   = 0;

            if (shouldShowCsvForDebug())
            {
                //  report periodic report timeouts
                dbgSerial.print  (F(":,"));
                dbgSerial.print  (2);
                dbgSerial.print  (F(","));
                dbgSerial.print  ((int32_t)millis());
                dbgSerial.print  (F(","));
                dbgSerial.print  ((int32_t)event->data1);
                dbgSerial.print  (F(","));
                dbgSerial.println((int32_t)event->data2);
            }

            /*  Loop thru all sensors, make the one with raw reports enabled blink if it is stable.
             *
             *  This works because only one sensor is ever enabled for raw reports at any time.
            */
            for (int sensorIndex = 0; sensorIndex < sensorType_MaxSensors; sensorIndex++)
            {
                if (rawSensorReportsEnable[sensorIndex])
                {
                    if (last_DRawVal == filteredSensorReading[sensorIndex])
                    {
                        switch (colorIndex)
                        {
                        case 0:
                            nextionSerial.print(F("adcCnts.pco=1024"));
                            colorIndex++;
                            break;
                        case 1:
                        default:
                            nextionSerial.print(F("adcCnts.pco=0"));
                            colorIndex = 0;
                            break;
                        }
                        nextionSerial.print(F("\xFF\xFF\xFF"));
                    }
                    else
                    {
                        last_DRawVal = filteredSensorReading[sensorIndex];
                        nextionSerial.print(F("D.Raw.val="));
                        nextionSerial.print(last_DRawVal);
                        nextionSerial.print(F("\xFF\xFF\xFF"));
//                      delay(POST_SERIAL_PAUSE_MSEC);
                        if (lastStable != (stabilityCount[sensorIndex] >= 5))
                        {
                            lastStable = (stabilityCount[sensorIndex] >= 5);
                            nextionSerial.print(F("D.Stable.val="));
                            nextionSerial.print((stabilityCount[sensorIndex] >= 5) ? 1 : 0);
                            nextionSerial.print(F("\xFF\xFF\xFF"));
//                          delay(POST_SERIAL_PAUSE_MSEC);
                        }
                    }
                }
            }
#ifndef _WIN32
            if (1)
                if (shouldShowCsvForDebug())
                {
                    //  report raw and filtered readings we actually view to a log file
                    dbgSerial.print  (F(":,"));
                    dbgSerial.print  (6);
                    dbgSerial.print  (F(","));
                    dbgSerial.print  (millis());
                    dbgSerial.print  (F(","));
                    dbgSerial.println((int32_t)currentRawSensorReading[sensorType_Weight]);
                    dbgSerial.print  (F(","));
                    dbgSerial.print  ((int32_t)filteredSensorReading[sensorType_Weight]);
                }
#endif      //  _WIN32

            //  Compute density in grams per cubic meter
            float   denominator = ((float)lastDisplayedValue[sensorType_DistanceLength] *
                                   (float)lastDisplayedValue[sensorType_DistanceWidth ] *
                                   (float)lastDisplayedValue[sensorType_DistanceHeight]);

            int16_t index;
            bool    allSteady = true;

            if (denominator >= 2.0)
            {
                totalDensity_kgPerMeter3 = (float)((float)lastDisplayedValue[sensorType_Weight] * 1000000.0) / denominator;

                float   densityFromRaw = densityFromRawmsmts_kgm3();

#ifdef  _WIN32
                printf("Dbg : OriginalDensity=%lf  NewDensity=%lf\n", (double)totalDensity_kgPerMeter3, (double)densityFromRaw);

                if (totalDensity_kgPerMeter3 >= 205.0 &&  totalDensity_kgPerMeter3 <= 209.0)
                {
                    printf("");     //  break here
                }
#endif  // _WIN32

                snapshotOldDensity = totalDensity_kgPerMeter3;
                snapshotNewDensity = densityFromRaw;

                //  Replace the density with the same value computed directly from raw measurements.  It seems more accurate.
                totalDensity_kgPerMeter3 = densityFromRaw;

                //  Quantitize the value with some hysteresis
                const   float   densityQuantization = 1.5;  //  Set to zero to disable quantization

                if (densityQuantization != 0.0)
                {
                    float   newTotalDensity_kgPerMeter3 = (float)((totalDensity_kgPerMeter3 + (densityQuantization / 2.0)) / densityQuantization + densityBiasValue);
                    long    integerizedDensity          = (long)newTotalDensity_kgPerMeter3;

                    newTotalDensity_kgPerMeter3 = (float)integerizedDensity;
                    newTotalDensity_kgPerMeter3 = newTotalDensity_kgPerMeter3 * densityQuantization;

                    if (densityBiasValue == 0.0)
                    {
                        //  DensityBiasValue allows the first report to put a finger on the scale to give density some hysteresis
                        if (newTotalDensity_kgPerMeter3 >= totalDensity_kgPerMeter3)
                        {
                            densityBiasValue = 0.5;
                        }
                        else
                        {
                            densityBiasValue = -0.5;
                        }

                        //  Recompute density with the new bias value
                        newTotalDensity_kgPerMeter3 = (float)((totalDensity_kgPerMeter3 + densityQuantization / 2.0) / densityQuantization + densityBiasValue);
                        integerizedDensity = (long)newTotalDensity_kgPerMeter3;

                        newTotalDensity_kgPerMeter3 = (float)integerizedDensity;
                        newTotalDensity_kgPerMeter3 = newTotalDensity_kgPerMeter3 * densityQuantization;
                    }
                    totalDensity_kgPerMeter3 = newTotalDensity_kgPerMeter3;
                }
            }

            bool    allSensorsAreTaring = true;

            for (index = 0; index < sensorType_MaxSensors; index++)
            {
#if   defined(FORCED_HEIGHT_VALUE)
                if (index == sensorType_DistanceHeight)
                {
                    //  Skip this sensor
                }
                else
#endif  //  FORCED_HEIGHT_VALUE

#if   defined(FORCED_WEIGHT_VALUE)
                if (index == sensorType_DistanceHeight)
                {
                    //  Skip this sensor
                }
                else
#endif  //  FORCED_WEIGHT_VALUE
                {
                    if (steadyValueCount[index] < 5)
                    {
                        allSteady = false;
                        showStableMark[index] = false;
                    }

                    if (taringState[index] != measState_taring)
                    {
                        allSensorsAreTaring = false;
                    }
                }
            }
            if (allSensorsAreTaring)
            {
//              dbgSerial.println(F("Stick Count density cleared"));
                sendEvent(tallyTrkEvt_DensityCleared, 0, 0);
            }

            if (finalMeasReportsEnabled  &&  shouldShowTareStateForDebug())
            {
                for (index = 0; index < sensorType_MaxSensors; index++)
                {
                    if (lastDisplayedTaringState[index] != taringState[index])
                    {
                        if (0  &&  index == sensorType_DistanceLength)
                        {
                            dbgSerial.print(F("Showing states "));
                        }

                        lastDisplayedTaringState[index] = taringState[index];
                        switch (index)
                        {
                        case sensorType_DistanceLength: nextionSerial.print(F("lstate.txt=\"" )); break;
                        case sensorType_DistanceWidth:  nextionSerial.print(F("wstate.txt=\"" )); break;
                        case sensorType_DistanceHeight: nextionSerial.print(F("hstate.txt=\"" )); break;
                        case sensorType_Weight:         nextionSerial.print(F("wtstate.txt=\"")); break;
                        default:                        nextionSerial.print(F("wtstate.txt=\"")); break;
                        }

                        switch(taringState[index])
                        {
                        case measState_unknown:     nextionSerial.print(F("?\"")); break;
                        case measState_taring:      nextionSerial.print(F("T\"")); break;
                        case measState_transition:  nextionSerial.print(F("X\"")); break;
                        case measState_measuring:   nextionSerial.print(F("M\"")); break;
                        default:                    nextionSerial.print(F("D\"")); break;
                        }
                        nextionSerial.print(F("\xFF\xFF\xFF"));

                        if (0  &&  index == sensorType_DistanceLength)
                        {
                            dbgSerial.print(index);
                            dbgSerial.print(F(" "));
                        }

                        if (0  &&  index == sensorType_DistanceLength)
                        {
                            dbgSerial.print ((char)('A' + taringState[index]));
                            dbgSerial.print ((char)('A' + lastDisplayedTaringState[index]));
                            dbgSerial.print (F(" "));
                            dbgSerial.println();
                        }
                    }
                    else
                    {
                    }
                }
            }
            else
            {
                for (index = 0; index < sensorType_MaxSensors; index++)
                {   //  Set unknown so we show info on first time going back to main screen
                    lastDisplayedTaringState[index] = measState_unknown;

                    if (0 &&  index == sensorType_Test)
                    {
                        dbgSerial.println(F("Clearing states"));
                    }
                }
            }

            if (0  &&  !allSteady)
            {   //  This compound statement is just for printing debug info
                for (index = 0; index < sensorType_MaxSensors; index++)
                {
                    if (steadyValueCount[index] < 5)
                    {
                        dbgSerial.print(F("_"));
                    }
                    else
                    {
                        switch (index)
                        {
                        case sensorType_DistanceLength: dbgSerial.print(F("L")); break;
                        case sensorType_DistanceWidth:  dbgSerial.print(F("W")); break;
                        case sensorType_DistanceHeight: dbgSerial.print(F("H")); break;
                        case sensorType_Weight:         dbgSerial.print(F("G")); break;
                        }
                    }
                }
//              dbgSerial.print(F("\n"));
            }

            if (finalMeasReportsEnabled)
            {
                //  Only send the density info if it has changed

                if (denominator < 2.0)
                {
                    densityString = String(F(" None "));        //  None1
                    snapshotOldDensity = 0.0;
                    snapshotNewDensity = 0.0;
                    debugHeight        = 0.0;
                    debugWidth         = 0.0;
                    debugLength        = 0.0;
                    debugWeight        = 0.0;
                }
                else if (!allSteady)
                {
//                  densityString = String(F("Stab ")) + (stabilityCount[0] > 5 ? "1" : "0") + (stabilityCount[1] > 5 ? "1" : "0") + (stabilityCount[2] > 5 ? "1" : "0") + (stabilityCount[3] > 5 ? "1" : "0") + index;
                    densityString = String(F(" None "));        //  None2
                    snapshotOldDensity = 0.0;
                    snapshotNewDensity = 0.0;
                }
                else
                {
                    densityString = String(totalDensity_kgPerMeter3, 1);    // using a float and the decimal places

                    //  Report the measurements that went into this density
                    sendEvent(binningEvt_SetCurrentStickLength,  lastDisplayedValue[sensorType_DistanceLength], 0);
                    sendEvent(binningEvt_SetCurrentStickWidth,   lastDisplayedValue[sensorType_DistanceWidth ], 0);
                    sendEvent(binningEvt_SetCurrentStickHeight,  lastDisplayedValue[sensorType_DistanceHeight], 0);
                    sendEvent(binningEvt_SetCurrentStickWeight,  lastDisplayedValue[sensorType_Weight        ], 0);
                    sendEvent(binningEvt_SetCurrentStickDensity, (int32_t)totalDensity_kgPerMeter3            , 0);

                    sendEvent(tallyTrkEvt_DensityPublished, (int32_t)(totalDensity_kgPerMeter3 * 10.0), 0);
                    sendEvent(timerEvt_cancelTimer, msmtMgrEvt_ForceDensityDisplayUpdate, 0);
                    sendEvent(timerEvt_startTimer,  msmtMgrEvt_ForceDensityDisplayUpdate, 50);
//                  dbgSerial.println(F("Stick Count density published"));

#ifdef  _WIN32
                    printf("DensityCmp,%d,%8.4f,%8.4f\n", millis(), snapshotOldDensity, snapshotNewDensity);
                    printf("");
#endif  //  _WIN32
                }

//              dbgSerial.println(F("Check density string"));
                densityString.toCharArray(tmpArray, sizeof(tmpArray));
                if (strcmp(oldDensityString, tmpArray) != 0)
                {
                    strcpy(oldDensityString, tmpArray);
#ifdef  _WIN32
                    printf("DensityTxt, %d, %s\n", millis(), tmpArray);
#else   //  _WIN32
                    nextionSerial.print(F("Density.txt=\""));
                    nextionSerial.print(densityString);
                    nextionSerial.print(F("\"\xFF\xFF\xFF"));
                    delay(POST_SERIAL_PAUSE_MSEC);
//                  dbgSerial.print(F("Showed density string "));
//                  dbgSerial.println(densityString);

                    if (shouldShowTareStateForDebug())
                    {
                        nextionSerial.print(F("stixRemain.txt=\""));
                        nextionSerial.print(tallyTrkObj_GetStickCount());
                        nextionSerial.print(F("\"\xFF\xFF\xFF"));
                        delay(POST_SERIAL_PAUSE_MSEC);
                        dbgSerial.println(F("Showed stick Count"));
                    }
                    sendEvent(timerEvt_startTimer, msmtMgrEvt_ReportMainScreenAccumulations, 60);
#endif  //  _WIN32
                }
            }
            else
            {
                densityString = String(F(" None "));            //  None3
                densityString.toCharArray(tmpArray, sizeof(tmpArray));
                strcpy(oldDensityString, tmpArray);
                snapshotOldDensity = 0.0;
                snapshotNewDensity = 0.0;
#ifndef _WIN32
                nextionSerial.print(F("Density.txt=\""));
                nextionSerial.print(densityString);
                nextionSerial.print(F("\"\xFF\xFF\xFF"));
                delay(POST_SERIAL_PAUSE_MSEC);
//              dbgSerial.println(F("Showed density string2"));

                if (shouldShowTareStateForDebug())
                {
                    nextionSerial.print(F("stixRemain.txt=\""));
                    nextionSerial.print(tallyTrkObj_GetStickCount());
                    nextionSerial.print(F("\"\xFF\xFF\xFF"));
                    delay(POST_SERIAL_PAUSE_MSEC);
                }
                sendEvent(timerEvt_startTimer, msmtMgrEvt_ReportMainScreenAccumulations, 60);
#endif // !_WIN32
            }

#ifdef  _WIN32
            //  This reports data for a density calculation
            if (showMeasNotRawCsv)
            {
                int     csvLineNumber = getLineCnt();
static          bool    showDensityHeader = true;


                if (csvLineNumber == 512)
                {
                    printf("");
                }

                if (showDensityHeader)
                {
                    showDensityHeader = false;
                    printf("\n:CSV"
                        ",timestamp"
                        ",LineNum"
                        ",lastStable"
                        ",denominator"
                        ",allSteady"
                        ",steadyValueCnt0"
                        ",steadyValueCnt1"
                        ",steadyValueCnt2"
                        ",steadyValueCnt3"
                        ",finalMeasReportsEnabled"
                        ",tareState0"
                        ",tareState1"
                        ",tareState2"
                        ",tareState3"
                        ",TotalDensity_kgPerM3"
                        ",lastDisplayValue0"
                        ",lastDisplayValue1"
                        ",lastDisplayValue2"
                        ",lastDisplayValue3"
                        ",densityString"
                        ",measStateGain0"
                        ",measStateGain1"
                        ",measStateGain2"
                        ",measStateGain3"
                        ",rawMeas0"
                        ",rawMeas1"
                        ",rawMeas2"
                        ",rawMeas3"
                        ",filtMeas0"
                        ",filtMeas1"
                        ",filtMeas2"
                        ",filtMeas3"
                        ",Tare0"
                        ",Tare1"
                        ",Tare2"
                        ",Tare3"
                        ",TareOffset0"
                        ",TareOffset1"
                        ",TareOffset2"
                        ",TareOffset3"
                        ",DisplayValLength"
                        ",DisplayValWidth"
                        ",DisplayValHeight"
                        ",DisplayValWeight"
                        ",filtNumerator0"
                        ",filtNumerator1"
                        ",filtNumerator2"
                        ",filtNumerator3"
                        ",filtDenomintor0"
                        ",filtDenomintor1"
                        ",filtDenomintor2"
                        ",filtDenomintor3"
                        ",oldDensity"
                        ",newDensity"
                        ",debugLength"
                        ",debugWidth"
                        ",debugHeight"
                        ",debugWeight"
                        "\n"
                    );
                }   //               1     3     5     7     9    11    13    15    17    19    21    23    25    27    29    31    33    35    37    39    41    43    45    47    49    51    53          55          57
                printf("\n:CSV,%06d,%d,%d,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%0.5f,%0.5f,%0.5f,%0.5f\n",
                        millis(),                   //     0
                        csvLineNumber++,            //     1
                        lastStable,                 //     2
                        denominator,                //     3 float
                        allSteady,                  //     4
                        steadyValueCount[0],        //     5
                        steadyValueCount[1],        //     6
                        steadyValueCount[2],        //     7
                        steadyValueCount[3],        //     8
                        finalMeasReportsEnabled,    //     9
                        taringState[0],             //    10
                        taringState[1],             //    11
                        taringState[2],             //    12
                        taringState[3],             //    13
                        totalDensity_kgPerMeter3,   //    14 float
                        lastDisplayedValue[0],      //    15
                        lastDisplayedValue[1],      //    16
                        lastDisplayedValue[2],      //    17
                        lastDisplayedValue[3],      //    18
                        densityString.c_str(),      //    19
                        measStateGain[0],           //    20
                        measStateGain[1],           //    21
                        measStateGain[2],           //    22
                        measStateGain[3],           //    23
                        currentRawSensorReading[0], //    24
                        currentRawSensorReading[1], //    25
                        currentRawSensorReading[2], //    26
                        currentRawSensorReading[3], //    27
                        filteredSensorReading[0],   //    28
                        filteredSensorReading[1],   //    29
                        filteredSensorReading[2],   //    30
                        filteredSensorReading[3],   //    31
                        showTareMark[0],            //    32
                        showTareMark[1],            //    33
                        showTareMark[2],            //    34
                        showTareMark[3],            //    35
                        getCalTarePoint(0),         //    36
                        getCalTarePoint(1),         //    37
                        getCalTarePoint(2),         //    38
                        getCalTarePoint(3),         //    39
                        findYFromCalibration(sensorType_DistanceLength, filteredSensorReading[sensorType_DistanceLength]),   //  40
                        findYFromCalibration(sensorType_DistanceWidth,  filteredSensorReading[sensorType_DistanceWidth]),    //  41
                        findYFromCalibration(sensorType_DistanceHeight, filteredSensorReading[sensorType_DistanceHeight]),   //  42
                        findYFromCalibration(sensorType_Weight,         filteredSensorReading[sensorType_Weight]),           //  43
                        (int32_t)lastNumeratorFiltSensor[0],    //  44
                        (int32_t)lastNumeratorFiltSensor[1],    //  45
                        (int32_t)lastNumeratorFiltSensor[2],    //  46
                        (int32_t)lastNumeratorFiltSensor[3],    //  47
                        (int32_t)lastDenomnatrFiltSensor[0],    //  48
                        (int32_t)lastDenomnatrFiltSensor[1],    //  49
                        (int32_t)lastDenomnatrFiltSensor[2],    //  50
                        (int32_t)lastDenomnatrFiltSensor[3],    //  51
                        snapshotOldDensity,                     //  52
                        snapshotNewDensity,                     //  53
                        debugLength,                            //  54
                        debugWidth,                             //  55
                        debugHeight,                            //  56
                        debugWeight                             //  57
                );
                showTareMark[0] = 0;
                showTareMark[1] = 0;
                showTareMark[2] = 0;
                showTareMark[3] = 0;
            }
#endif // !_WIN32
        }
        break;

    case msmtMgrEvt_ReportRawSensorMsmt:
        {
            /*  data1 : raw sensor value, in ADC counts.  Treat as signed 32-bit value
                data2 : Sensor index.  [0..3]
             */
            int64_t     numerator,
                        denominator;

            int32_t currentNoiseEstimate,
                    lowNoiseGainMultiplier,
                    sameDirectionGainMultiplier;
#ifndef _WIN32
            if (1)
            if (shouldShowCsvForDebug())
            {
                //  report raw sensor readings to a log file
                dbgSerial.print  (F(":,"));
                dbgSerial.print  (1);
                dbgSerial.print  (F(","));
                dbgSerial.print  (millis());
                dbgSerial.print  (F(","));
                dbgSerial.print  ((int32_t)event->data1);
                dbgSerial.print  (F(","));
                dbgSerial.println((int32_t)event->data2);
            }
#endif      //  _WIN32

#ifdef      DONT_DO
            dbgSerial.print(F("RawReport #"));
            dbgSerial.print(event->data2);
            dbgSerial.print(F(" = "));
            dbgSerial.print(event->data1);
            dbgSerial.print(F("   "));
            dbgSerial.println(event->data1, HEX);
#endif
            if (1  &&  event->data2 == sensorType_Test)
            {
                dbgSerial.println(F("ReportRawMsmt"));
            }

            if ((event->data1 & 0x00003FF) == 0x0003FF)
            {
                //  Last 10 bits are one's, ignore this
                return;
            }

            //  Store the new measurement for future use
            int16_t     sensorIndex = event->data2;   //  Make a quick local copy.
            int32_t     workingCurrentRawSensorReading = event->data1;  //  Keep a local copy so we can ignore extremes

            currentRawSensorReading [sensorIndex] = workingCurrentRawSensorReading;
            measReportCntInThisState[sensorIndex]++;

            assert(sensorIndex >= 0 && sensorIndex <= 3);

            if (1 && sensorIndex == sensorType_Test)
            {
                printf("");
            }

            if (((workingCurrentRawSensorReading < -7500000  ||  workingCurrentRawSensorReading > 7500000)  &&  sensorIndex != sensorType_DistanceLength)  ||
                ((workingCurrentRawSensorReading < -8300000  ||  workingCurrentRawSensorReading > 8300000)  &&  sensorIndex == sensorType_DistanceLength))
            {   /*  The raw reading is too close to the max/min value, probably a noise spike
                    or the sensor reporting an error by going to max range.  Ignore the raw
                    reading by setting the working value to the last filtered value, effectively replacing the bogus
                    raw point with the most recent filtered point.
                */
                workingCurrentRawSensorReading = filteredSensorReading[sensorIndex];
            }
            else
            {
                //  Store the new raw sample in the noise estimator
                rawSampleNoiseEstimate[sensorIndex].addNewValue(workingCurrentRawSensorReading);
            }

            //  Check to see if we can be more responsive
//          rawSampleNoiseEstimate[sensorIndex].trimOldIfNeeded();

            if (1  &&  sensorIndex == sensorType_Test)
            {
                printf("");
            }

            if (0  &&  sensorIndex == sensorType_Test)
            {
                dbgSerial.print(F("TestSensor report "));
                dbgSerial.print(currentRawSensorReading[sensorIndex]);
                dbgSerial.println();
            }

            intx32_t rawReading_x32 = ((intx32_t)workingCurrentRawSensorReading) << 32;
            intx32_t delta_x32 = filteredSensorReading_x32[sensorIndex] - rawReading_x32;

            if (0)
            {
                dbgSerial.print((int32_t)filteredSensorReading[sensorIndex]);
                dbgSerial.print(F(" "));

                dbgSerial.print((int32_t)workingCurrentRawSensorReading);
                dbgSerial.print(F(" "));
                dbgSerial.print((int32_t)(filteredSensorReading[sensorIndex] - workingCurrentRawSensorReading));
                dbgSerial.print(F(" "));

                dbgSerial.print((int32_t)(delta_x32 >> 32));
                dbgSerial.print(F(" "));
            }

            if (delta_x32 < 0)
            {
                sameDirectionNudge[sensorIndex]--;
                if (sameDirectionNudge[sensorIndex] < 0)
                {
                    sameDirectionNudge[sensorIndex] -= (sameDirectionNudge[sensorIndex] <= -20) ? 3 : 1;
                }
                else
                {
                    sameDirectionNudge[sensorIndex] -= 20;
                }
            }
            else
            {
                sameDirectionNudge[sensorIndex]++;
                if (sameDirectionNudge[sensorIndex] > 0)
                {
                    sameDirectionNudge[sensorIndex] += (sameDirectionNudge[sensorIndex] >= 20) ? 3 : 1;
                }
                else
                {
                    sameDirectionNudge[sensorIndex] += 20;
                }
            }

            //  Enforce sanity limits
            sameDirectionNudge[sensorIndex] = max(-120, min(120, sameDirectionNudge[sensorIndex]));
            filterScaling_Pctg[sensorIndex] = max(   1, min(quantizationLimit[sensorIndex] - 1, filterScaling_Pctg[sensorIndex]));

            /*  The noise estimate is really variance (standard deviation squared).
            */
            numerator   = filterScaling_Pctg[sensorIndex];
            denominator = quantizationLimit [sensorIndex];

            currentNoiseEstimate = rawSampleNoiseEstimate[sensorIndex].getShortNoiseEstimate();
            if (currentNoiseEstimate > 0)
            {
                lowNoiseGainMultiplier = 400 / currentNoiseEstimate;  //  Heuristically determined
            }
            else
            {
                lowNoiseGainMultiplier = 5;
            }

            sameDirectionGainMultiplier = labs(sameDirectionNudge[sensorIndex]) / 13;
            //  Don't apply the first step
            if (sameDirectionGainMultiplier == 2) sameDirectionGainMultiplier = 1;

            sameDirectionGainMultiplier *= 5;
            sameDirectionGainMultiplier /= 3;

            if (0  &&  sensorIndex == sensorType_Test)
            {
                dbgSerial.println(F("Deciding which direction"));
            }

            lowNoiseGainMultiplier      = max(1, min(    5, lowNoiseGainMultiplier     ));
            sameDirectionGainMultiplier = max(1, min(   15, sameDirectionGainMultiplier));
            measStateGain[sensorIndex]  = max(1, min(10000, measStateGain[sensorIndex] ));

            if (sensorIndex == sensorType_Test)
            {   //  Breakpoint for a NOP
                printf("");
            }

            if (0)
            {
                dbgSerial.print(F("Same direction cnt="));
                dbgSerial.println(sameDirectionNudge[sensorIndex]);
                dbgSerial.print(F("gain "));
                dbgSerial.println(sameDirectionGainMultiplier);
            }

            int32_t     stdDevToUse = rawSampleNoiseEstimate[sensorIndex].getShortStdDeviation_x100(),
                        deltaToUse  = labs((int32_t)(delta_x32 >> 32));

            bool        applyLinearCorrection = true;

            int         myCurrentSampleCount = rawSampleNoiseEstimate[sensorIndex].currentSampleCount();
            int         stdDeviationThreshold = 4;

            if (myCurrentSampleCount > 15)
            {
                //  Make it harder to exercise
                stdDeviationThreshold = 6;
            }

            if (sensorIndex == sensorType_Test)
            {   //  Breakpoint for a NOP
                printf("");
            }

            //  Add a rule that if number of standard deviations away from raw is high, filtered is just replace with average, but only
            //  if taring or measuring
//          if ((rawSampleNoiseEstimate[sensorIndex].currentSampleCount() > 10) &&
            if ((myCurrentSampleCount >= shortNoiseEstimater::shortHistoryDepth) &&
               ((100 * deltaToUse) > (stdDeviationThreshold * stdDevToUse))  &&
                (taringState[sensorIndex] == measState_taring  ||  taringState[sensorIndex] == measState_measuring))
            {
                int32_t         lastPublicFilteredReading = filteredSensorReading[sensorIndex];
                bool            useThisSensor = true;

                applyLinearCorrection = false;
                if (0  &&  sensorIndex == sensorType_DistanceLength)
                {
                    //  Don't apply this rule if the difference is within 6mm.  The IFM length sensor has poor resolution
                    int32_t     scalingAmount = getCalAdcCountsPerUnit_x1000(sensorIndex);
                    if ((scalingAmount * 60) / 10000 > deltaToUse)
                    {
                        useThisSensor         = false;
                        applyLinearCorrection = true;
                    }
                }

                //  Debug only - uncomment this line to skip special rule for IFM laser sensor
                //  useThisSensor = true;

                if (sensorIndex == 0)
                {
                    printf("");
                }

                //  TODO - Maybe use this window only for lasers?  Load cells need to do this
                if (myCurrentSampleCount > 40)
                {   //  The window has passed, don't use taring strategy
                    useThisSensor = false;
                    applyLinearCorrection = true;
                }

                if (1  &&  sensorIndex == sensorType_Test  &&  useThisSensor)
                {
                    dbgSerial.println(F("Option1 taken"));
                }

                if (0  &&  sensorIndex == sensorType_Test  &&  useThisSensor)
                {
                    dbgSerial.print(F("Override entry "));
                    dbgSerial.print(rawSampleNoiseEstimate[sensorIndex].currentSampleCount());
                    dbgSerial.print(F(" "));
                    dbgSerial.print(rawSampleNoiseEstimate[sensorIndex].getStdDeviation_x100());
                    dbgSerial.println();
                }

//              rawSampleNoiseEstimate[sensorIndex].resetNoiseEstimate();

                if (useThisSensor)
                {
                    if (sensorIndex == sensorType_Test)
                    {   //  Breakpoint for a NOP
                        printf("");
                    }

                    //  We are more than four std Deviations away, so the average raw value is better than any
                    //  filtered value we will compute.  Use the average raw sample value as the new filtered value
                    int32_t     newFilterSensorReading = rawSampleNoiseEstimate[sensorIndex].getShortRecentAverage(),
                                filterDifference       = newFilterSensorReading - filteredSensorReading[sensorIndex],
                                newTarePoint           = getCalTarePoint(sensorIndex) + filterDifference;

                    filteredSensorReading    [sensorIndex] = newFilterSensorReading;
                    filteredSensorReading_x32[sensorIndex] = (intx32_t)filteredSensorReading[sensorIndex] << 32;

                    if (sensorIndex == sensorType_Test)
                    {   //  Breakpoint for a NOP
                        printf("");
                    }
                    if (newTarePoint > 249200 && sensorIndex == 3)
                    {
                        printf("");
                    }

                    if (taringState[sensorIndex] == measState_taring)
                    {
                        setCurrentTarePoint(sensorIndex, newTarePoint);
                        tareHistory[sensorIndex].clear(0);
                        rawSampleNoiseEstimate[sensorIndex].resetNoiseEstimate();
                    }
                }
                if (1  &&  filteredSensorReading[sensorIndex] > lastPublicFilteredReading  &&  useThisSensor)
                {
                    if (0  &&  sensorIndex == sensorType_Test)
                    {
                        dbgSerial.print(F("Apply override - pos  "));
                        dbgSerial.print(filteredSensorReading[sensorIndex]);
                        dbgSerial.print(F("  "));
                        dbgSerial.print(lastPublicFilteredReading);
                        dbgSerial.println();
                    }
                    //  Add half a ADC count
                    filteredSensorReading_x32[sensorIndex] = filteredSensorReading_x32[sensorIndex] + (intx32_t)0x80000000;
                }
                else if (1  &&  filteredSensorReading[sensorIndex] < lastPublicFilteredReading  &&  useThisSensor)
                {
                    if (0  &&  sensorIndex == sensorType_Test)
                    {
                        dbgSerial.print(F("Apply override - neg  "));
                        dbgSerial.print(filteredSensorReading[sensorIndex]);
                        dbgSerial.print(F("  "));
                        dbgSerial.print(lastPublicFilteredReading);
                        dbgSerial.println();
                    }
                    //  Subtract half a ADC count
                    filteredSensorReading_x32[sensorIndex] = filteredSensorReading_x32[sensorIndex] - (intx32_t)0x80000000;
                }
                else if (0  &&  sensorIndex == sensorType_Test  &&  useThisSensor)
                {
                    dbgSerial.println(F("Nothing Appied"));
                }

                if (rawSampleNoiseEstimate[sensorIndex].currentSampleCount() > 20  &&  useThisSensor)
                {   //  This is a safety check that keeps us from dumping the history buffer unless we are
                    //  smart enough to  make the  decision correctly
                    if (sensorType_Test == sensorIndex)
                    {
                        dbgSerial.println(F("currentSampleCount > 20, call resetNoiseEst()"));
                    }
                    rawSampleNoiseEstimate[sensorIndex].resetNoiseEstimate();
                    sameDirectionNudge[sensorIndex] = 0;
                }
            }

            /*  applyLinearCorrection is a boolean needed to simplify logic.  The previous if statement
                can decide and undecide to do something in a complicated way.  It was just easier to
                introduce a new temporary variable to help determine if this next section should run, or not.
            */
            if (applyLinearCorrection)
            {
                int32_t     lastPublicFilteredSensorReading = filteredSensorReading[sensorIndex];

                if (0  &&  sensorIndex == sensorType_Test)
                {
                    dbgSerial.println(F("Option2 taken"));
                }

                if (sensorIndex == sensorType_Test)
                {
                    printf("");
                }
                //  Assemble the  numerator and denominator, then apply them to the delta and add it to the last
                //  filtered value
                int64_t         usableNumerator = numerator;

                numerator = numerator * lowNoiseGainMultiplier * sameDirectionGainMultiplier * measStateGain[sensorIndex];

                //  Add a 1 point penalty.  If we go below 1, then we hit a floor later on
                numerator -= 1;

                denominator = max(1, denominator);
                numerator   = max(1, min(denominator - 1, numerator));

                //  Constrain sameDirMult limit so it can't push the numerator too high
                const   int64_t     ratioLimit = 10;     //  Increase to reduce the effect of sameDirectionGainMultiplier
                if ((denominator / numerator) < ratioLimit)
                {
                    if (sensorType_Test == sensorIndex)
                    {
                        printf("");
                    }
                    sameDirectionGainMultiplier = min(sameDirectionGainMultiplier, (int32_t)(denominator / (ratioLimit * usableNumerator * lowNoiseGainMultiplier * measStateGain[sensorIndex])));
                    sameDirectionGainMultiplier = max(1, sameDirectionGainMultiplier);      //   Don't go below zero
                    numerator                   = usableNumerator * lowNoiseGainMultiplier * sameDirectionGainMultiplier * measStateGain[sensorIndex];
                    numerator                   = max( 1, min(denominator - 1, numerator));
                }

                if (shouldUseGainFloorAfterStateChange(sensorIndex))
                {   /*  Keep the numerator large after state transition for a few measurement reports.  This lets
                        the final setting occur quicker.
                    */
                    numerator = max(numerator, getMaxDivisorScalingLimit(sensorIndex) / 10);
                }

                if (0  &&  sensorType_Test == sensorIndex)
                {
                    float       scalingFraction = ((float)numerator) / ((float)denominator);
                    dbgSerial.print(F("Scaling="));
                    dbgSerial.print(scalingFraction, 6);
                    dbgSerial.println();
                }

                int64_t     offset;

                if (numerator > 200)
                {
                    offset = (delta_x32 / denominator) * numerator;  //  avoid overflow, divide first
                }
                else
                {
                    offset = (delta_x32 * numerator) / denominator;  //  keep precision, divide last
                }
                filteredSensorReading_x32[sensorIndex] -= offset;
                filteredSensorReading    [sensorIndex]  = (int32_t)(filteredSensorReading_x32[sensorIndex] >> 32);

                //  Round toward middle to give a bit of hysteresis
                if (1  &&  lastPublicFilteredSensorReading < filteredSensorReading[sensorIndex])
                {
                    if (0  &&  sensorIndex == sensorType_Test)
                    {
                        for (int spikes = 0; spikes < 20; spikes++)
                        {
                            dbgSerial.println(10);
//                          dbgSerial.println( 0);
                        }
                    }
                    filteredSensorReading_x32[sensorIndex] = ((intx32_t)filteredSensorReading[sensorIndex] << 32) + (intx32_t)0x80000000;
//                  dbgSerial.println(F("Apply wrap - pos  "));
                }
                else if (1  &&  lastPublicFilteredSensorReading > filteredSensorReading[sensorIndex])
                {
                    if (0  &&  sensorIndex == sensorType_Test)
                    {
                        for (int spikes = 0; spikes < 20; spikes++)
                        {
                            dbgSerial.println(15);
                            dbgSerial.println(20);
                        }
                        dbgSerial.print(F("lastPublic "));
                        dbgSerial.print(lastPublicFilteredSensorReading);
                        dbgSerial.print(F(" "));
                        dbgSerial.print(filteredSensorReading[sensorIndex]);
                    }
                    filteredSensorReading_x32[sensorIndex] = ((intx32_t)filteredSensorReading[sensorIndex] << 32) + (intx32_t)0x80000000;
//                  dbgSerial.println(F("Apply wrap - neg  "));
                }
                else
                {
                    /*  Push the fractional part toward mid-range a wee bit on each sample.  This is like a constant
                        current drain that ensures the fractional part ends to wander back to the middle of the range
                        it is in if there is no clear direction from the sensor.  This value is about 0.001% of 2^31.
                    */
                    int32_t     fractionalPart = (int32_t)filteredSensorReading_x32[sensorIndex];

                    if (1  && fractionalPart > 0)
                    {
                        filteredSensorReading_x32[sensorIndex] += 160000;
                    }
                    else if (1  && fractionalPart < 0)  //  Left as a discrete test so we can disable easily
                    {
                        filteredSensorReading_x32[sensorIndex] -= 160000;
                    }
                }
            }

            if (0  &&  sensorIndex == noiseRptActiveSensor  &&  (int32_t)rawSampleNoiseEstimate[sensorIndex].getNoiseEstimate() < 100000)
            {
                static  int8_t     printed = 0;
                if (!printed)
                {
                    printed++;
//                  dbgSerial.print("Delta Numerator Denominator Noise^2 Noise SameDirGain LoNoiseGain" );
                    dbgSerial.print("Numerator Denominator SameDirGain LoNoiseGain FullClock fraction");
                    dbgSerial.println();
                }

//              dbgSerial.print((int32_t)(delta_x32 >> 32));
//              dbgSerial.print(F("   "));                       //   <<--- 3 spaces
                dbgSerial.print(min(22, (int32_t)numerator));
                dbgSerial.print(F(" "));
                dbgSerial.print((int32_t)denominator - getMaxDivisorScalingLimit(sensorIndex));
                dbgSerial.print(F(" "));
//              dbgSerial.print((int32_t)filteredSensorReading[sensorIndex]);
//              dbgSerial.print(F(" "));
//              dbgSerial.print((int32_t)rawSampleNoiseEstimate[sensorIndex].getNoiseEstimate());
//              dbgSerial.print(F(" "));
//              dbgSerial.print(sqrt((int32_t)rawSampleNoiseEstimate[sensorIndex].getNoiseEstimate()) * 10);
//              dbgSerial.print(F(" "));
                dbgSerial.print(sameDirectionGainMultiplier);
                dbgSerial.print(F(" "));
                dbgSerial.print(lowNoiseGainMultiplier);
                dbgSerial.print(F(" "));


                int32_t     fullClockValue = (int32_t)((((filteredSensorReading_x32[sensorIndex] - filtX32StdevBias) >> 14) * 125) >> 15);

                fullClockValue = max(-20, min( 20, fullClockValue));

                dbgSerial.print(fullClockValue / 1000);
                dbgSerial.print(F("."));
                if (fullClockValue <  10) dbgSerial.print(F("0"));
                if (fullClockValue < 100) dbgSerial.print(F("0"));
                dbgSerial.print(fullClockValue % 1000);
                dbgSerial.print(F(" "));

//                                            ((((int32_t)(filteredSensorReading_x32[sensorIndex] >> 8)) *  200) >> 24)  This gives +/- 150

                int32_t     fractionToPrint = ((int32_t)filteredSensorReading_x32[sensorIndex]) >> 16;

                if (1)
                {
                    fractionToPrint += 32768;
                    fractionToPrint *= 2000;
                    fractionToPrint /= 65536;

                    //  Flip the data inside out to align visually
                    fractionToPrint += 1000;
                    if (fractionToPrint >= 2000) fractionToPrint -= 2000;

                    dbgSerial.print(fractionToPrint / 100);
                    dbgSerial.print(F("."));
                    if (labs(fractionToPrint % 100) < 10)
                    {
                        dbgSerial.print(F("0"));
                    }
                    dbgSerial.print((int)labs(fractionToPrint % 100));
                }
                dbgSerial.println();
            }

            /*  filterScaling_Pctg[] increases filtering as it decreases, ie - maximum filtering when percentage is the lowest.
                It can grow up to the limit of quantizationLimit[] - 1
            */

            /*  Decide how stable we are and adjust filtering accordingly  */
            int16_t currentAmount    = findYFromCalibration(sensorIndex, currentRawSensorReading[sensorIndex]),
                    filteredAmount   = findYFromCalibration(sensorIndex, filteredSensorReading  [sensorIndex]),
                    amountDifference = (int16_t)labs(currentAmount - filteredAmount);

            if (sensorIndex == sensorType_DistanceWidth)
            {
                if (filteredAmount > 10)
                {
                    printf("");
                }
            }

#ifdef      DONT_DO
            if (currentAmount != -32768)
            {
                dbgSerial.print(F("Sensor "));
                dbgSerial.print(event->data2);
                dbgSerial.print(F("  "));
                dbgSerial.print(currentAmount);
                dbgSerial.print(F("  "));
                dbgSerial.print(filteredAmount);
                dbgSerial.println();
            }
#endif      //  DONT_DO

#ifdef      DONT_DO
            dbgSerial.print(F("\ncurrAmount="));
            dbgSerial.print(currentAmount);

            dbgSerial.print(F("   filtAmount="));
            dbgSerial.print(filteredAmount);

            dbgSerial.print(F("   WD="));
            dbgSerial.print(amountDifference);

            dbgSerial.print(F(" stability="));
            dbgSerial.println(stabilityCount[sensorIndex]);

            printCalArray((sensorType_t)sensorIndex);
#endif

            if (amountDifference <= 2)
            {
                //  Sensor seems more stable, filter more by reducing the percentage
                filterScaling_Pctg[sensorIndex] *= getFilterIncreasePctg(sensorIndex);
                filterScaling_Pctg[sensorIndex] /= 100;
                if (filterScaling_Pctg[sensorIndex] <= 1) filterScaling_Pctg[sensorIndex] = 1;    // Limit the percentage to be above zero

                stabilityCount[sensorIndex]++;
                stabilityCount[sensorIndex] = min(100, stabilityCount[sensorIndex]);
            }
            else if (amountDifference >= 5)
            {   //  Release the filter so it responds fast.  Bigger Pctg means bigger numerator, more of the raw measurement is used
                int32_t     difference = quantizationLimit[sensorIndex] - filterScaling_Pctg[sensorIndex];
                difference                      *= getFilterReleasePctg(sensorIndex);  //  Should be a value [0..100]
                difference                      /= 100;
                filterScaling_Pctg[sensorIndex] += difference;
                 if (filterScaling_Pctg[sensorIndex] > quantizationLimit[sensorIndex] - 1)
                {   // Limit the percentage to be quantizationLimit or below
                    filterScaling_Pctg[sensorIndex] = quantizationLimit[sensorIndex] - 1;
                }

//              dbgSerial.print(F("Release\n"));
                //  Make stability disappear quickly
                stabilityCount[sensorIndex] -= 7;
                stabilityCount[sensorIndex]  = max(1, stabilityCount[sensorIndex]);
            }

            if (filteredAmount <= 10  &&  filteredAmount != -32768  &&  stabilityCount[sensorIndex] >= 30)
            {
                if (validCalExists(sensorIndex))
                {
                    //  Don't auto-tare if we don't have at least 2 calibration points to know if we are stable
                    //  This happens inside validCalExists() above.


                    if (sensorIndex == sensorType_Test)
                    {
                        printf("");
                    }

                    //  We have a valid stable low value, autotare the sensor
                    if (tareHistory[sensorIndex].getItemCount() >= 1)
                    {
                        if (setCurrentTarePoint(sensorIndex, tareHistory[sensorIndex].getOldest()))
                        {
                            showTareMark[sensorIndex] = 2;
                        }
                    }
                    if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("SetTare_5"));

                    stabilityCount[sensorIndex] = 15;
                }
            }

            if ((sensorIndex == noiseRptActiveSensor)  &&  (noiseRptActiveSensor >= 0)  &&  (noiseRptActiveSensor <= sensorType_MaxSensors))
            {
                if (1)
                {
                    int32_t     rawDifference,
                                filtDifference;

                    if (rawStdDevBias == UNASSIGNED_STDEV_START)
                    {
                        //  Use filtered value as the baseline
                        //  rawStdDevBias = currentRawSensorReading[sensorIndex];
                        rawStdDevBias  = filtStdDevBias;
                    }

                    if (filtStdDevBias == UNASSIGNED_STDEV_START)
                    {
                        filtStdDevBias   = filteredSensorReading[sensorIndex];
                        rawStdDevBias    = filtStdDevBias;
                        filtX32StdevBias = filteredSensorReading_x32[sensorIndex] - ((intx32_t)5 << 32);
                        if (0)
                        {
                            dbgSerial.print(F("Set two noise zero biases "));
                            dbgSerial.print(filtStdDevBias);
                            dbgSerial.print(F("   index="));
                            dbgSerial.print(sensorIndex);
                            dbgSerial.println();
                        }
                    }

                    rawDifference  = currentRawSensorReading[sensorIndex] - rawStdDevBias;
                    filtDifference = filteredSensorReading  [sensorIndex] - filtStdDevBias;

                    //  Compute statistics for knowing standard deviation
                    if (rawDifference < 20000)
                    {
                        rawStdevCnt++;
                        rawSquaredStdevSum += rawDifference * rawDifference;
                        rawStdevSum        += rawDifference;
                        if (rawSquaredStdevSum > 1000000000)
                        {   //  Move all the integer accumulated values into floating point values
                            rawSquaredStdevAccumulator += (float)rawSquaredStdevSum;
                            rawStdevAccumulator        += (float)rawStdevSum;
                            rawStdevAccumCnt           += (float)rawStdevCnt;
                            rawSquaredStdevSum          = 0;
                            rawStdevSum                 = 0;
                            rawStdevCnt                 = 0;
                        }
                    }

                    if (filtDifference < 20000) //  Ignore if too big, the squared value
                    {
                        filtStdevCnt++;
                        filtSquaredStdevSum += filtDifference * filtDifference;
                        filtStdevSum        += filtDifference;
                        if (filtSquaredStdevSum > 1000000000)
                        {   //  Move all the integer accumulated values into floating point values
                            filtSquaredStdevAccumulator += (float)filtSquaredStdevSum;
                            filtStdevAccumulator        += (float)filtStdevSum;
                            filtStdevAccumCnt           += (float)filtStdevCnt;
                            filtSquaredStdevSum          = 0;
                            filtStdevSum                 = 0;
                            filtStdevCnt                 = 0;
                        }
                    }

                    graphPlotPoint(1, rawDifference );      //  Diff'ed from baseline, so it's always zero'ed
                    graphPlotPoint(2, filtDifference);      //  Diff'ed from baseline, so it's always zero'ed
                    graphPlotPoint(0, showTareMark[sensorIndex] ? -80 : -100);

#ifndef _WIN32
                    showTareMark[sensorIndex] = 0;
#endif

                    if (0)
                    {
                        dbgSerial.print(F("RawDifference="));
                        dbgSerial.print(rawDifference);
                        dbgSerial.print(F("     filtDiff="));
                        dbgSerial.print(filtDifference);
                        dbgSerial.print(F("\n"));
                    }

                    {
                        static      uint8_t reportCnt = 0;

                        if (reportCnt++ > 20)
                        {
                            reportCnt = 0;

                            //  Compute and report std Deviations
                            float   workingRawStdevAccumCnt            = rawStdevAccumCnt            + (float)rawStdevCnt,
                                    workingRawStdevAccumulator         = rawStdevAccumulator         + (float)rawStdevSum,
                                    workingRawSquaredStdevAccumulator  = rawSquaredStdevAccumulator  + (float)rawSquaredStdevSum,

                                    workingFiltStdevAccumCnt           = filtStdevAccumCnt           + (float)filtStdevCnt,
                                    workingFiltStdevAccumulator        = filtStdevAccumulator        + (float)filtStdevSum,
                                    workingFiltSquaredStdevAccumulator = filtSquaredStdevAccumulator + (float)filtSquaredStdevSum;

                            float   rawStdDev = (float)sqrt( (float)(workingRawStdevAccumCnt * workingRawSquaredStdevAccumulator    - workingRawStdevAccumulator  * workingRawStdevAccumulator ) /
                                                             (float)(workingRawStdevAccumCnt * (workingRawStdevAccumCnt - 1) ) );

                            float   filtStdDev = (float)sqrt( (float)(workingFiltStdevAccumCnt * workingFiltSquaredStdevAccumulator - workingFiltStdevAccumulator * workingFiltStdevAccumulator) /
                                                              (float)(workingFiltStdevAccumCnt * (workingFiltStdevAccumCnt - 1) ) );

                            nextionSerial.print(F("rawStdev.val="));
                            nextionSerial.print((int)(rawStdDev * 100));
                            nextionSerial.print(F("\xFF\xFF\xFF"));

                            nextionSerial.print(F("filtStdev.val="));
                            nextionSerial.print((int)(filtStdDev * 100));
                            nextionSerial.print(F("\xFF\xFF\xFF"));
                        }
                    }

                    {
                        uint8_t     currentFilterScalingReported = (uint8_t)((100 * (quantizationLimit[sensorIndex] - filterScaling_Pctg[sensorIndex])) / quantizationLimit[sensorIndex]);

                        if (lastFilterScalingReported != currentFilterScalingReported)
                        {
                            nextionSerial.print(F("stability.val="));
                            nextionSerial.print((int)currentFilterScalingReported );
                            nextionSerial.print(F("\xFF\xFF\xFF"));

                            if (0)
                            {
                                dbgSerial.print(F("Pctg="));
                                dbgSerial.print(filterScaling_Pctg[sensorIndex]);
                                dbgSerial.print(F("  QuantLimit="));
                                dbgSerial.print(quantizationLimit[sensorIndex] );
                                dbgSerial.print(F("  new="));
                                dbgSerial.print(currentFilterScalingReported   );
                                dbgSerial.print(F("  old="));
                                dbgSerial.print(lastFilterScalingReported      );
                                dbgSerial.print(F("\n"));
                            }
                            lastFilterScalingReported = currentFilterScalingReported;
                        }
                    }
                }
            }

//  Moved from above
            {
                char        dValAssignmentStr[16],
                            mainScreenStr    [16];

                switch (sensorIndex)
                {
                case sensorType_DistanceLength: strcpy_P(dValAssignmentStr, PSTR("D.L.val=" )); strcpy_P(mainScreenStr, PSTR("length.val=")); break;
                case sensorType_DistanceWidth : strcpy_P(dValAssignmentStr, PSTR("D.W.val=" )); strcpy_P(mainScreenStr, PSTR("width.val=" )); break;
                case sensorType_DistanceHeight: strcpy_P(dValAssignmentStr, PSTR("D.H.val=" )); strcpy_P(mainScreenStr, PSTR("height.val=")); break;
                case sensorType_Weight:         strcpy_P(dValAssignmentStr, PSTR("D.Wt.val=")); strcpy_P(mainScreenStr, PSTR("grams.val=" )); break;
                }

                if (sensorIndex == sensorType_Test)
                {
                    printf("");
                    if (filteredAmount > 20)
                    {
                        printf("");
                    }
                }

                int16_t     newValue = filteredAmount;

                if (sensorIndex == sensorType_Test)
                {
                    printf("");
                }

#if   defined(FORCED_HEIGHT_VALUE)
                if (sensorIndex == sensorType_DistanceHeight)
                {   //  Override
                    newValue = findYFromCalibration(sensorIndex, filteredSensorReading[sensorType_DistanceWidth]);
                    newValue = (lastDisplayedValue[sensorType_DistanceWidth] > 20) ? FORCED_HEIGHT_VALUE : 0;
                    stabilityCount  [sensorType_DistanceHeight] = stabilityCount[sensorType_DistanceWidth];
                    steadyValueCount[sensorType_DistanceHeight] = 6;
                    taringState     [sensorType_DistanceHeight] = taringState[sensorType_DistanceWidth];
                    measReportCntInThisState[sensorType_DistanceHeight] = 0;
                    if (newValue >= 20)
                    {
                        printf("");
                    }
                    if (newValue != 0)
                    {
                        printf("");
                    }
                }
#endif  //  defined(FORCED_HEIGHT_VALUE)

#if   defined(FORCED_WEIGHT_VALUE)
                if (sensorIndex == sensorType_Weight)
                {   //  Override
                    newValue = findYFromCalibration(sensorIndex, filteredSensorReading[sensorType_DistanceWidth]);
                    newValue = (lastDisplayedValue[sensorType_DistanceWidth] > 20) ? FORCED_WEIGHT_VALUE : 0;
                    stabilityCount  [sensorType_Weight] = stabilityCount[sensorType_DistanceWidth];
                    steadyValueCount[sensorType_Weight] = 6;
                    taringState     [sensorType_Weight] = taringState[sensorType_DistanceWidth];
                    measReportCntInThisState[sensorType_Weight] = 0;
                    if (newValue >= 20)
                    {
                        printf("");
                    }
                    if (newValue != 0)
                    {
                        printf("");
                    }
                }
#endif  //  defined(FORCED_WEIGHT_VALUE)

                if (lastDisplayedValue[1] < -500)
                {
                    printf("");
                }

                switch (taringState[sensorIndex])
                {
                default:
                    if (sensorIndex == sensorType_Test)
                    {
                        printf("");
                    }
                    taringState           [sensorIndex] =  measState_transition;
                    newValueIsIdenticalCnt[sensorIndex] = 0;       //  Prepare for new state
                    newValueBeyond10Cnt[sensorIndex]    = 0;       //  Prepare for a return to this state
                    measStateGain[sensorIndex]          = 10000;   //  high gain until we settle
                    densityBiasValue                    = 0.0;
                    measReportCntInThisState[sensorIndex] = 0;

                    tareHistory[sensorIndex].clear(0);
                    if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("State: default->transition 1"));
                    break;

                case measState_taring:
                    {
                        //  Count measurements in a row > 10 mm/grams
                        if (sensorIndex == sensorType_Test)
                        {
                            printf("");
                        }
                        int32_t     newCurrentSensorAmount  = labs(findYFromCalibration(sensorIndex, currentRawSensorReading[sensorIndex]));
                        int32_t     newFilteredSensorAmount = labs(findYFromCalibration(sensorIndex, filteredSensorReading  [sensorIndex]));
                        int32_t     difference = newCurrentSensorAmount - newFilteredSensorAmount;

                        if (newCurrentSensorAmount > 10)
                        {
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }

                            newValueBeyond10Cnt[sensorIndex]++;
                            newValueIsIdenticalCnt[sensorIndex] = 0;
                            newValueBeyond10Cnt[sensorIndex] = min(10, newValueBeyond10Cnt[sensorIndex]);
                        }
                        else
                        {
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            newValueBeyond10Cnt[sensorIndex] = 0;
                        }

                        newValueBeyond10Cnt[sensorIndex] = min(10, newValueBeyond10Cnt[sensorIndex]);
                        measStateGain[sensorIndex] = 1;

                        if (newValueBeyond10Cnt[sensorIndex] >= 2)
                        {   //  2 in a row >= 10 mm/grams, move to transition
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            taringState[sensorIndex] = measState_transition;
                            newValueIsIdenticalCnt[sensorIndex] = 0;       //  Prepare for new state
                            newValueBeyond10Cnt[sensorIndex] = 0;       //  Prepare for a return to this state
                            measStateGain[sensorIndex] = 10000;       //  high gain until we settle
                            densityBiasValue = 0.0;
                            measReportCntInThisState[sensorIndex] = 0;

                            tareHistory[sensorIndex].clear(0);
                            if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("State: taring->transition  2"));
                        }

                        /*  Tare immediately if current value is below tare.  Make sure we are in the taring state
                         *  and also that the value is tiny and looks like drift, not big and looks like noise.
                         *
                        */
                        int32_t     sensorCurrentTareAdcValue = getCalTarePoint(sensorIndex);
                        int16_t     tareAmount = findYFromCalibration(sensorIndex, sensorCurrentTareAdcValue);

                        if (taringState[sensorIndex]     == measState_taring  &&
                            (tareAmount - currentAmount) <= 2                 &&
                            (tareAmount - currentAmount) >= 0                 &&
                            currentAmount                != -32768            &&
                            tareAmount                   != -32768)
                        {
                            assert(sensorIndex >= 0  &&  sensorIndex <= 3);
                            if (isCalSlopeSignPositive(sensorIndex))
                            {   //  Slope is positive
                                if (filteredSensorReading[sensorIndex] < sensorCurrentTareAdcValue && newValue != -32768)
                                {   //  Latest filtered ADC value is below current tare, so it
                                    if (sensorIndex == sensorType_Test)
                                    {
                                        printf("");
                                    }
                                    if (setCurrentTarePoint(sensorIndex, 0))
                                    {
                                        showTareMark[sensorIndex] = 3;
                                    }
                                    if (0 && sensorIndex == sensorType_Test) dbgSerial.println(F("SetTare_1"));

                                    if (0 && sensorIndex == sensorType_Test)
                                    {
                                        dbgSerial.println(F("BelowZero+"));
                                    }
                                }
                            }
                            else
                            {   //  Slope is negative
                                if (filteredSensorReading[sensorIndex] > sensorCurrentTareAdcValue && newValue != -32768)
                                {   //  Latest filtered ADC value is above current tare, so it
                                    if (sensorIndex == sensorType_Test)
                                    {
                                        printf("");
                                    }

                                    if (setCurrentTarePoint(sensorIndex, 0))
                                    {
                                        showTareMark[sensorIndex] = 4;
                                    }
                                }
                            }
                        }
                        else
                        {
                            /*  Some reason prevents us from taring, so allow a breakpoint
                            */
                            printf("");
                        }

                        //  Count runs of identical values
                        if (newValue == lastDisplayedValue[sensorIndex] && newValue >= -10)     //  newValue is in units of mm/grams
                        {
                            newValueIsIdenticalCnt[sensorIndex]++;
                            newValueIsIdenticalCnt[sensorIndex] = min(10, newValueIsIdenticalCnt[sensorIndex]);
                        }
                        else
                        {
                            newValueIsIdenticalCnt[sensorIndex] = 0;
                        }


                        if ( newValueIsIdenticalCnt[sensorIndex] >= 5  ||
                            (newValueIsIdenticalCnt[sensorIndex] >= 3  &&  sensorIndex == sensorType_Weight) //  Require less for weight
                           )
                        {   //  We have enough identical readings, log a tare value in history buffer
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            tareHistory[sensorIndex].add(filteredSensorReading[sensorIndex]);
                        }

                        //  Always update tare from the oldest
                        if (tareHistory[sensorIndex].getItemCount() >= 1)
                        {
//                          setCurrentTarePoint(sensorIndex, tareHistory[sensorIndex].getOldest());
                        }
                    }
                    break;

                case measState_transition:
                    if (sensorIndex == sensorType_Test)
                    {
                        printf("");
                    }
                    //  Count identical measurements in a row
                    if (0 &&  sensorIndex == sensorType_Test)
                    {
                        dbgSerial.print(F("State:transition"));
                        dbgSerial.print(newValue);
                        dbgSerial.print(F(", "));
                        dbgSerial.print(lastDisplayedValue[sensorIndex]);
                        dbgSerial.print(F(", "));
                        dbgSerial.print(newValueIsIdenticalCnt[sensorIndex]);
                        dbgSerial.print(F(", "));
                        dbgSerial.print(measStateGain[sensorIndex]);
                    }

                    {
                        //  Check for newValue is greater than a small negative value, as some lasers go negative and stay there
                        if (newValue == lastDisplayedValue[sensorIndex] && newValue >= -10)
                        {
//                          dbgSerial.print(newValueIsIdenticalCnt[sensorIndex] + 100);
//                          dbgSerial.print(F(", "));

                            newValueIsIdenticalCnt[sensorIndex]++;
//                          dbgSerial.print(newValueIsIdenticalCnt[sensorIndex] + 100);
//                          dbgSerial.print(F(", bump"));
                        }
                        else
                        {
                            newValueIsIdenticalCnt[sensorIndex] = 0;
//                          dbgSerial.print(newValueIsIdenticalCnt[sensorIndex]);
//                          dbgSerial.print(F(", zero"));
                        }
                    }
//                  newValueIsIdenticalCnt[sensorIndex] += (newValue == lastDisplayedValue[sensorIndex]  &&  newValue >= -10) ? 1 : -newValueIsIdenticalCnt[sensorIndex];
                    if (newValueIsIdenticalCnt[sensorIndex] > 10)
                    {
//                      dbgSerial.print(F(", "));
                        newValueIsIdenticalCnt[sensorIndex] = 10;
//                      dbgSerial.print(F(", cap"));
                    }

                    if (sensorIndex == sensorType_Weight)
                    {
                        printf("");
                    }
                    //  Starts at 1000, keep measStateGain at 10 or more while in transition
                    if (measStateGain[sensorIndex] > 10) measStateGain[sensorIndex] -= 1000;
                    if (measStateGain[sensorIndex] < 10) measStateGain[sensorIndex]  =  500;

                    if (newValueIsIdenticalCnt[sensorIndex] >= 2)
                    {   //  2 in a row are identical, move to either measuring or taring
                        if (newValue > 10)
                        {
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            taringState       [sensorIndex] = measState_measuring;
                            measStateGain     [sensorIndex] = 1;     //  Low gain, we are stable
                            newValueBelow10Cnt[sensorIndex] = 0;
                            measReportCntInThisState[sensorIndex] = 0;
                            rawSampleNoiseEstimate[sensorIndex].resetNoiseEstimate();
//                          dbgSerial.print(F(", moveToMeasuring"));
                            if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("State: transition->measuring3"));
                        }
                        else
                        {   //  This is change from 'transition' to 'taring' state
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            taringState[sensorIndex] = measState_taring;
                            measReportCntInThisState[sensorIndex] = 0;

// #define TARE_UPON_ENTRY_TO_TARING_STATE
#ifdef  TARE_UPON_ENTRY_TO_TARING_STATE
                            if (setCurrentTarePoint(sensorIndex, 0))
                            {
                                showTareMark[sensorIndex] = 5;
                            }
#endif // TARE_UPON_ENTRY_TO_TARING_STATE

                            rawSampleNoiseEstimate[sensorIndex].resetNoiseEstimate();
                            if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("SetTare_3"));
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }
                            tareHistory[sensorIndex].add(filteredSensorReading[sensorIndex]);
                            measStateGain[sensorIndex] = 1;     //  Low gain, we are stable
//                          dbgSerial.print(F(", moveToTaring"));
                            if (0  &&  sensorIndex == sensorType_Test)
                            {
                                dbgSerial.print(F("State: transition->taring  4  "));
                                dbgSerial.print(findYFromCalibration(sensorType_Test, filteredSensorReading[sensorType_Test]));
                                dbgSerial.println(F(""));
                            }
                        }
                        newValueBeyond10Cnt   [sensorIndex] = 0;        //  Prepare for new state
                        newValueIsIdenticalCnt[sensorIndex] = 0;        //  Prepare for a return to this state
                    }

                    if (0 &&  sensorIndex == sensorType_Test)
                    {
                        dbgSerial.print(F(", NewValIdentical="));
                        dbgSerial.print(newValueIsIdenticalCnt[sensorIndex]);
                        dbgSerial.println();
                    }
                    break;

                case measState_measuring:
                    if (sensorIndex == sensorType_Test)
                    {
                        printf("");
                    }
                    //  Count consecutive measurements with difference > 10
                    int16_t         localCurrentRaw = findYFromCalibration(sensorIndex, currentRawSensorReading[sensorIndex]);
                    newValueBeyond10Cnt[sensorIndex] += (labs(localCurrentRaw - newValue) > 10) ? 1 : -newValueBeyond10Cnt[sensorIndex];
                    newValueBeyond10Cnt[sensorIndex]  = min(10, newValueBeyond10Cnt[sensorIndex]);

                    newValueBelow10Cnt [sensorIndex] += (newValue < 10  &&  newValue > -32768) ? 1 : -newValueBelow10Cnt[sensorIndex];
                    newValueBelow10Cnt [sensorIndex]  = min(10, newValueBelow10Cnt[sensorIndex]);

                    if (newValueBeyond10Cnt[sensorIndex] >= 2  || newValueBelow10Cnt[sensorIndex] >= 2)
                    {   //  2 in a row with difference > 10 mm/grams, move to transition
                        if (sensorIndex == sensorType_Test)
                        {
                            printf("");
                        }
                        taringState           [sensorIndex] =  measState_transition;
                        measReportCntInThisState[sensorIndex] =  0;
                        newValueIsIdenticalCnt[sensorIndex] =  0;       //  Prepare for new state
                        newValueBeyond10Cnt   [sensorIndex] =  0;       //  Prepare for a return to this state
                        measStateGain         [sensorIndex] = 10000;    //  high gain until we settle
                        if (0  &&  sensorIndex == sensorType_Test) dbgSerial.println(F("State: measuring->transition5"));
                    }
                    break;
                }

                if (sensorIndex == sensorType_Test)
                {
                    printf("");
                }

                if (lastDisplayedValue[sensorIndex] != newValue)
                {
                    lastDisplayedValue[sensorIndex] = newValue;
#ifndef _WIN32
                    nextionSerial.print(dValAssignmentStr);
                    nextionSerial.print(newValue);
                    nextionSerial.print(F("\xFF\xFF\xFF"));

                    if (finalMeasReportsEnabled)
                    {   //  This goes on the main screen for the width display
                        nextionSerial.print(mainScreenStr);
                        nextionSerial.print(newValue);
                        nextionSerial.print(F("\xFF\xFF\xFF"));
                    }
                    delay(POST_SERIAL_PAUSE_MSEC);
#endif // !_WIN32

                    //  The display value didn't match new, steadyCount must decrease.  We just have to decide how much
                    if (steadyValueCount[sensorIndex] <= 5)
                    {
                        steadyValueCount[sensorIndex] = 0;
                        showStableMark  [sensorIndex] = false;
                    }
                    else
                    {
                        steadyValueCount[sensorIndex]--;
                    }
                }
                else
                {
                    //  The value is unchanged since last time, so it is steady
                    steadyValueCount[sensorIndex]++;
                    if (steadyValueCount[sensorIndex] == 5)
                    {
                        //  We just transitioned to steady state on this sensor, attempt a screen update
                        sendEvent(msmtMgrEvt_PeriodicReportTimeout, 0, 0);
                    }
                    else if (steadyValueCount[sensorIndex] > 5)
                    {
                        steadyValueCount[sensorIndex] = 6;
                        if (newValue < 10 && newValue != -32768)  //  Under 10 grams/mm and not invalid
                        {
                            if (sensorIndex == sensorType_Test)
                            {
                                printf("");
                            }

                            if (tareHistory[sensorIndex].getItemCount() >= 1)
                            {
                                if (setCurrentTarePoint(sensorIndex, tareHistory[sensorIndex].getOldest()))
                                {
                                    showTareMark[sensorIndex] = 6;
                                }
                            }
                            if (0  &&  sensorIndex == sensorType_Test)
                            {
                                dbgSerial.print(F("SetTare_4   newValue="));
                                dbgSerial.println(newValue);
                            }
                        }
                    }
                    if (steadyValueCount[sensorIndex] >= 5)
                    {
                        showStableMark[sensorIndex] = true;
                    }
                }
            }

#ifdef _WIN32
            //  This reports data for each single raw sensor measurement
            if (!showMeasNotRawCsv &&  sensorType_Test >= sensorType_First  &&  sensorType_Test < sensorType_MaxSensors  &&  sensorType_Test == sensorIndex)
            {
                int     csvLineNumber = getLineCnt();
static          bool    showSensorHeader    = true;

                if (showSensorHeader)
                {
                    showSensorHeader = false;
                    printf("\n:CSV"
                        ",timestamp"
                        ",LineNum"                  //   1
                        ",Raw"
                        ",Filt"                     //   3
                        ",tareState"
                        ",tareOccured"              //   5
                        ",Stable"
                        ",LastDisplayedTareState"   //   7
                        ",SteadyValCnt"
                        //                      ",Numerator"
                        ",Denominator"              //   9
                        ",isIdentical"
                        ",scalingPctg"              //  11
                        ",quantLimit"
                        ",sameDirNudge"             //  13
                        ",measStateGain"
                        ",isIdenticalCnt"           //  15
                        ",beyond10Cnt"
                        ",Numerator"                //  17
                        ",Density"
                        ",DisplayValLength"         //  19
                        ",DisplayValWidth"
                        ",DisplayValHeight"         //  21
                        ",DisplayValWeight"
                        ",reportedRaw"              //  23
                        ",reportedfiltered"
                        ",lowNoiseGainMultiplier"   //  25
                        ",sameDirectionGainMultiplier"
                        ",tarePoint"                //  27
                        "\n"
                    );
                }   //            0  1     3     5     7       9 10 11    13    15      17    19    21    23    25    27
                printf("\n:CSV,%06d,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%d,%d,%d,%d,%d,%d,%d,%lld,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    millis(),                                     //    0
                    csvLineNumber++,                              //    1
                    currentRawSensorReading[sensorType_Test],     //    2
                    filteredSensorReading[sensorType_Test],       //    3
                    taringState[sensorType_Test],                 //    4
                    showTareMark[sensorType_Test],                //    5
                    showStableMark[sensorType_Test] ? 6 : 0,      //    6
                    lastDisplayedTaringState[sensorType_Test],    //    7
                    steadyValueCount[sensorType_Test],            //    8
                    denominator,                                  //    9 long long
                    newValueIsIdenticalCnt[sensorType_Test],      //   10
                    filterScaling_Pctg[sensorType_Test],          //   11
                    quantizationLimit[sensorType_Test],           //   12
                    sameDirectionNudge[sensorType_Test],          //   13

                    measStateGain[sensorType_Test],               //   14
                    newValueIsIdenticalCnt[sensorType_Test],      //   15
                    newValueBeyond10Cnt[sensorType_Test],         //   16
                    numerator,                                    //   17
                    totalDensity_kgPerMeter3,                     //   18
                    findYFromCalibration(sensorType_DistanceLength, filteredSensorReading[sensorType_DistanceLength]),   //  19
                    findYFromCalibration(sensorType_DistanceWidth,  filteredSensorReading[sensorType_DistanceWidth]),    //  20
                    findYFromCalibration(sensorType_DistanceHeight, filteredSensorReading[sensorType_DistanceHeight]),   //  21
                    findYFromCalibration(sensorType_Weight,         filteredSensorReading[sensorType_Weight]),           //  22
                    lastReportedRawValue,                         //   23
                    lastReportedFilteredValue,                    //   24
                    lowNoiseGainMultiplier ,                      //   25
                    sameDirectionGainMultiplier,                  //   26
                    getCalTarePoint(sensorType_Test)              //   27
                );
                showTareMark[sensorType_Test] = 0;
            }
            else
            {
                if (sensorType_Test >= sensorType_MaxSensors)
                printf("no valid sensor\n");
            }
#endif  //  _WIN32

            //  Save the most recent numerator and denominator used for filtered calculations
            lastNumeratorFiltSensor[event->data2] = numerator;
            lastDenomnatrFiltSensor[event->data2] = denominator;
        }
        break;

    case msmtMgrEvt_ReportPing:
        dbgSerial.println("msmtMgrEvt_ReportPing");
        break;

    case msmtMgrEvt_ReportMainScreenAccumulations:
        showMainScreenBftValue();
        break;

    default:
        break;
    }
}


int32_t getLastFilteredSensorReading(sensorType_t sensor)
{
    int32_t   returnValue = 0;

    if (sensor >= 0  &&  sensor < sensorType_MaxSensors)
    {
        returnValue = filteredSensorReading[sensor];
    }
    return returnValue;
}
