

#include <stdint.h>
#include <assert.h>

#include "commonHeader.h"
#include "tallyTracker.h"
#include "accumulationMgr.h"


#ifndef _WIN32
#include <arduino.h>
#else
#include "win32shims.h"
extern EEPROM_t EEPROM;
extern  int32_t eventTimestamp;

#endif // _WIN32


#include "win32shims.h"

#ifndef   _WIN32
#include <EEPROM.h>

#endif



#include "calibration.h"
#include "msmtMgrEvtHandler.h"

#define CALIBRATION_VERSION   4

#ifndef _WIN32
extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;
#else
static  HardwareSerial      dbgSerial,
                            nextionSerial;

#endif // _WIN32


//#define SHOW_ALL_CAL_DEBUG

#ifdef  SHOW_ALL_CAL_DEBUG
#define ENABLE_CAL_PRINT
#define SHOW_FINDY_DEBUG
#define SHOW_CAL_DEBUG
#endif  //  SHOW_ALL_CAL_DEBUG

#define    sensorType_Test      (sensorType_DistanceLength)

/*  Storing or updating a new calibration point requires storing the current tare value for
    that sensor.  Three values allows correction for wandering tare points without having
    to store the necessary information every time the tare point updates.

    Rule : Whenever an ADC (X) value is used, it must subtract the tare value taken at the time
           the ADC value was captured _before_ using it for anything.

    1)  To record new calibration point: Store X, Y, and current tare for that sensor.
        Set current RAM tare to current tare in EEPROM.

    2)  To re-calibrate : Keep same Y value, update X and tare.

    3)  To tare again : update only RAM tare to current filtered sensor reading.

    4)  To use calibration points : The X value must be changed before using to account
        for a shift in tare.  The formula should be:

        newCalbrationXValue = (calibrationTareInRam - calibationEpromXValue) + storedX

    When using calibration point, a correction is computed by subtracting the current
    tare (in RAM) from the stored tare point (in EEPROM) to correct for differences
    in tare.

    Calibration X values (ADC counts) are corrected by subtracting the correct tare offset
    before use.
*/
typedef struct
{
    int32_t X;      //  ADC counts, signed 24-bit,
    int16_t Y;      //  grams or millimeters - We find this.
} *pCalPtr_t, calPoint_t;


typedef struct
{
    int16_t         calPointCnt;
    calPoint_t      calPoint[MAX_CAL_POINTS_PER_SENSOR];
} *pCalPointArray_t, calPointArray_t;

static  calPointArray_t      calPointArray[sensorType_MaxSensors];
static  int32_t              tareOffset   [sensorType_MaxSensors];    //  Keep current tare values in RAM to avoid excessive EEPROM writing
static  bool                 calSlopeSignIsPositive[sensorType_MaxSensors];

/*  These are the intermediate values needed to calculate Density directly from
    ADC counts.
*/
static  int32_t currentMXplusB_tareOffset[sensorType_MaxSensors];
static  float   currentMXplusB_Slope[sensorType_MaxSensors],
                currentMXplusB_Intercept[sensorType_MaxSensors];

float   debugWeight,        //  Global, so they can be printed
        debugHeight,
        debugLength,
        debugWidth;

static void moveCalPointsUp(sensorType_t sensor, int16_t itemIndex);
static void moveCalPointsDown(sensorType_t sensor, int16_t itemIndex);
void addCalPoint(sensorType_t sensor, int32_t newX, int16_t newY);
void deleteCalPointWithX(sensorType_t sensor, int32_t oldX);
void deleteCalPointWithIndex(sensorType_t sensor, int16_t index);
void deleteAllCalibration(sensorType_t sensor);
int32_t getCalPoint_Y_byIndex(sensorType_t sensor, int16_t index);
int32_t getCalPoint_X_byIndex(sensorType_t sensor, int16_t index);
int16_t findYFromCalibration(sensorType_t sensor, int32_t incomingX);
void printCalArray(sensorType_t sensor);
void addCalOffset(sensorType_t sensor, int32_t newTarePoint);
bool setCurrentTarePoint(sensorType_t sensor, int32_t newTare);
int16_t getCalIndexOfY(sensorType_t sensor, int16_t yToFind);
void writeCalDataToFlash(void);
void readCalDataFromFlash(bool setStickcount);
int32_t getLastFilteredSensorReading(sensorType_t sensor);
bool validCalExists(sensorType_t sensor);
int32_t getCalTarePoint(sensorType_t sensor);
bool  isCalSlopeSignPositive(sensorType_t sensor);
int32_t getCalAdcCountsPerUnit_x1000(sensorType_t sensor);

static  bool  displaySlopeAndIntercept,
              showCalibrationCalcSteps;

void enableCalibrationCalcPrinting(void)
{
    showCalibrationCalcSteps = true;
}


void disableCalibrationCalcPrinting(void)
{
    showCalibrationCalcSteps = false;
}


void showSlopeAndIntercept()
{
    displaySlopeAndIntercept = true;
}


void printCalArray(sensorType_t sensor)
{

//  DMMDBG = Enable both of these
//#define ENABLE_CAL_PRINT
//#define   PRINT_CAL_ARRAY


#ifndef  ENABLE_CAL_PRINT
#define ENABLE_CAL_PRINT
#endif

//#define PRINT_CAL_ARRAY
#if  defined(ENABLE_CAL_PRINT) ||  defined(PRINT_CAL_ARRAY)
    if (shouldShowCsvForDebug()  &&  showCalibrationCalcSteps)
    {
        pCalPointArray_t    calArray = &calPointArray[sensor];
        dbgSerial.print(F("  sensorIndex="));
        dbgSerial.print(sensor);
        dbgSerial.print(F("  PtCnt="));
        dbgSerial.print(calArray->calPointCnt);
        dbgSerial.print(F("         tareOffset="));
        dbgSerial.print(tareOffset[sensor]);
        dbgSerial.println(F(""));

        int   limit = calArray->calPointCnt;
        if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
        for (int16_t index = 0; index < limit; index++)
        {
            dbgSerial.print(F("  "));
            dbgSerial.print(index);
            dbgSerial.print(F(" X="));
            dbgSerial.print((int32_t)(calArray->calPoint[index].X));
            dbgSerial.print(F(" Y="));

            dbgSerial.println(calArray->calPoint[index].Y);
        }
        dbgSerial.println();
    }
#endif  //  ENABLE_CAL_PRINT
}


int checkCalTablesForFunnyBusiness(int callingLine)
{
    int     returnValue = 0,  // Returns non-zero if something was wrong
            passCnt = 0;


//  dbgSerial.println(F("checkCalTablesForFunnyBusiness entry"));

    while (passCnt < 2  &&  returnValue != 0)
    {
        passCnt++;
        returnValue = 0;
        for (sensorType_t sensor = sensorType_First; sensor < sensorType_MaxSensors; sensor++)
        {
            if (calPointArray[sensor].calPointCnt < 0  ||  calPointArray[sensor].calPointCnt > MAX_CAL_POINTS_PER_SENSOR)
            {
                returnValue = __LINE__;
#if    defined(ENABLE_CAL_PRINT)  ||  defined(SHOW_FUNNY_BUSINESS)
                dbgSerial.print(F("Funny- Sensor="));
                dbgSerial.print(sensor);
                dbgSerial.print(F("  PtCnt="));
                dbgSerial.print(calPointArray[sensor].calPointCnt);
                dbgSerial.print(F("  Caller="));
                dbgSerial.println(callingLine);
#endif  //  defined(ENABLE_CAL_PRINT)  ||  defined(SHOW_FUNNY_BUSINESS)
                calPointArray[sensor].calPointCnt = 0;    //  Return to sanity
            }

            for (int calPt = 0; calPt < calPointArray[sensor].calPointCnt; calPt++)
            {
                if ((calPointArray[sensor].calPoint[calPt].Y < 0  ||  calPointArray[sensor].calPoint[calPt].Y > 2000)
                    ||   calPointArray[sensor].calPoint[calPt].Y == 555    //  Special rules - just OR in more cases
                  )
                {
                    returnValue = __LINE__;
//#define  SHOW_FUNNY_BUSINESS
#if    defined(ENABLE_CAL_PRINT)  ||  defined(SHOW_FUNNY_BUSINESS)
                    dbgSerial.print(F("Funny- Sensor="));
                    dbgSerial.print(sensor);
                    dbgSerial.print(F("   PtCnt="));
                    dbgSerial.print(calPointArray[sensor].calPointCnt);
                    dbgSerial.print(F("   Cal#="));
                    dbgSerial.print(calPt);
                    dbgSerial.print(F("   Y="));
                    dbgSerial.print(calPointArray[sensor].calPoint[calPt].Y);
                    dbgSerial.print(F("  Caller="));
                    dbgSerial.println(callingLine);
                    printCalArray(sensor);
#endif  //  defined(ENABLE_CAL_PRINT)  ||  defined(SHOW_FUNNY_BUSINESS)
                    moveCalPointsUp(sensor, calPt);
                }
            }
        }
    }

    if (returnValue != 0)
    {
        nextionSerial.print(F("D.nextion.txt=\"FunnyBiz "));
        nextionSerial.print(returnValue);
        nextionSerial.print(F("  "));
        nextionSerial.print(callingLine);
        nextionSerial.print(F("\"\xFF\xFF\xFF"));
    }

//  dbgSerial.println(F("checkCalTablesForFunnyBusiness exit"));

    return returnValue;
}


int32_t getCalTarePoint(sensorType_t sensor)
{
    return tareOffset[sensor];
}


int32_t getCalAdcCountsPerUnit_x1000(sensorType_t sensor)
{
    //  Returns the number of ADC counts per mm or gram, times 1000.  The scaling
    //  is to preserve resolution for low count sensors.
    //
    //  This works by dividing the largest cal point reading.
    int     calPntIndex = calPointArray[sensor].calPointCnt - 1;

    return (calPointArray[sensor].calPoint[calPntIndex].X * 1000) / calPointArray[sensor].calPoint[calPntIndex].Y;
}


bool  isCalSlopeSignPositive(sensorType_t sensor)
{
    return calSlopeSignIsPositive[sensor];
}


bool validCalExists(sensorType_t sensor)
{
    bool    returnValue = false;
    if (sensor >= 0  &&  sensor < sensorType_MaxSensors)
    {
        //  For now, a valid calibration means two or more points exist
        if (calPointArray[sensor].calPointCnt >= 2)
        {
            returnValue = true;
        }
    }

    checkCalTablesForFunnyBusiness(__LINE__);

    return returnValue;
}

/*  Remove the item at the index location and move all values below it up.  */
void moveCalPointsUp(sensorType_t sensor, int16_t itemIndex)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];
    int16_t             index = itemIndex;

//  #define SHOW_MOVEUP_DEBUG
#if   defined(SHOW_CAL_DEBUG)  ||  defined(SHOW_MOVEUP_DEBUG)
    dbgSerial.println(F("Entering moveCalUp"));

    dbgSerial.print(F("before starting  sensor="));
    dbgSerial.print(sensor);
    dbgSerial.print(F("  itemIndex="));
    dbgSerial.print(itemIndex);
    dbgSerial.println();

    printCalArray(sensor);
#endif  //  SHOW_CAL_DEBUG

    checkCalTablesForFunnyBusiness(__LINE__);

    if (sensor < 0  ||  sensor >= sensorType_MaxSensors  ||  itemIndex < 0  || itemIndex >= MAX_CAL_POINTS_PER_SENSOR)
    {
        //  Invalid inputs, just ignore all
        return;
    }

    int   limit = MAX_CAL_POINTS_PER_SENSOR - 2;
    while (index < limit)
    {
        calArray->calPoint[index] = calArray->calPoint[index + 1];
#if  defined(SHOW_CAL_DEBUG) ||  defined(SHOW_MOVEUP_DEBUG)
        dbgSerial.print("index=");
        dbgSerial.println(index);
        printCalArray(sensor);
#endif  //  SHOW_CAL_DEBUG
        index++;
    }

    //  Make sure last calPoint is always well behaved with zeros, just in case
    calArray->calPoint[MAX_CAL_POINTS_PER_SENSOR - 1].X    = 0;
    calArray->calPoint[MAX_CAL_POINTS_PER_SENSOR - 1].Y    = 0;

    if (calArray->calPointCnt > 0)
    {
        calArray->calPointCnt--;
    }

#if   defined(SHOW_CAL_DEBUG)  ||  defined(SHOW_MOVEUP_DEBUG)
    dbgSerial.println("After movingUp");
    printCalArray(sensor);
#endif  //  SHOW_CAL_DEBUG

    checkCalTablesForFunnyBusiness(__LINE__);
}

static void moveCalPointsDown(sensorType_t sensor, int16_t itemIndex)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];
    int16_t             index     = MAX_CAL_POINTS_PER_SENSOR - 2;

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.println(F("moveCalDown entry"));
#endif  //  SHOW_CAL_DEBUG

    if (sensor < 0  ||  sensor >= sensorType_MaxSensors  ||  itemIndex < 0  || itemIndex >= MAX_CAL_POINTS_PER_SENSOR)
    {
        //  Invalid inputs, just ignore all
        return;
    }

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("tare offset="));
    dbgSerial.println(tareOffset[sensor]);

    dbgSerial.println(F("MoveDown() : before inserting"));
    printCalArray(sensor);
#endif  //  SHOW_CAL_DEBUG

    checkCalTablesForFunnyBusiness(__LINE__);

    /*  Start at the back end  */
    while (index >= itemIndex)
    {
        calArray->calPoint[index + 1] = calArray->calPoint[index];
#ifdef  SHOW_CAL_DEBUG
        dbgSerial.print(F("index="));
        dbgSerial.println(index);
        printCalArray(sensor);
#endif  //  SHOW_CAL_DEBUG
        index--;
    }

    if (calArray->calPointCnt < MAX_CAL_POINTS_PER_SENSOR)
    {
        calArray->calPointCnt++;
    }

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.println(F("MoveDown() : after inserting"));

    dbgSerial.print(F("tare offset="));
    dbgSerial.println(tareOffset[sensor]);

    printCalArray(sensor);

    dbgSerial.print(F("final moveDown tare offset="));
    dbgSerial.println(tareOffset[sensor]);
#endif  //  SHOW_CAL_DEBUG
    checkCalTablesForFunnyBusiness(__LINE__);
}


void forceCalPointPair(sensorType_t sensorIndex, int index, int32_t newX, int16_t newY)
{
    pCalPointArray_t    calArray = &calPointArray[sensorIndex];

    if (calArray->calPointCnt < index + 1)
    {
        calArray->calPointCnt = calArray->calPointCnt + 1;
    }

    calArray->calPoint[index].X = newX;
    calArray->calPoint[index].Y = newY;
}


/*  Inserts in order by finding the correct spot and moving everything
    below that down.  Anything on the bottom gets dropped off.
*/
void addCalPoint(sensorType_t sensor, int32_t newX, int16_t newY)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];
    bool                wasInserted = false;
    int16_t             index;

    if (shouldShowCsvForDebug())
    {
        dbgSerial.print(F("addCalPoint()  sensor="));
        dbgSerial.print(sensor);
        dbgSerial.print(F("  PointCnt="));
        dbgSerial.print(calArray->calPointCnt);
        dbgSerial.print(F("  newX="));
        dbgSerial.print(newX);
        dbgSerial.print(F("  newTare="));
        dbgSerial.print(tareOffset[sensor]);
        dbgSerial.print(F("  newY="));
        dbgSerial.println(newY);
    }

    checkCalTablesForFunnyBusiness(__LINE__);

    if (calArray->calPointCnt == 0)
    {
        if (tareOffset[sensor] == 0)
        {
            /*  We don't have a valid tare point.  This is bad, we don't want
             *   to add a calibration point without a tare
             */
            if (shouldShowCsvForDebug())
            {
                dbgSerial.println(F("No valid tare, ignoring the new cal point"));
            }
            return;
        }

        /*  Empty array, insert and return  */
        calArray->calPointCnt   = 1;
        calArray->calPoint[0].X = getLastFilteredSensorReading(sensor) - tareOffset[sensor];
        calArray->calPoint[0].Y = newY;

        /*  Make sure the zero point is zero in both dimensions  */
        if (newY == 0) calArray->calPoint[0].X = 0;

        sendEvent(msmtMgrEvt_ReportCalibrationSet1, sensor, newX);
        sendEvent(msmtMgrEvt_ReportCalibrationSet2, sensor, newY);
        wasInserted = true;
#ifdef  SHOW_CAL_DEBUG
        dbgSerial.println(F("Inserted first point"));
#endif  //  SHOW_CAL_DEBUG
    }

    int   limit = calArray->calPointCnt;
    if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
    for (index = 0; index < limit  &&  !wasInserted; index++)
    {
        if (calArray->calPoint[index].Y == newY)
        {
            /*  Replace based on Y, that's the value in millimeters/grams  */
            if (shouldShowCsvForDebug())
            {
                dbgSerial.print(F("Replacing "));
                dbgSerial.print(calArray->calPoint[index].X);
                dbgSerial.print(F(" with "));
                dbgSerial.println(newX);
            }
            //  .Y is already set, we are replacing the point by updating just .X
            calArray->calPoint[index].X = getLastFilteredSensorReading(sensor) - tareOffset[sensor];

            if (calArray->calPoint[index].Y == 0)
            {
                //  This is setting a new tare point, make sure both X and Y are zero.
                calArray->calPoint[index].X = 0;
            }
            sendEvent(msmtMgrEvt_ReportCalibrationSet1, sensor, newX);
            sendEvent(msmtMgrEvt_ReportCalibrationSet2, sensor, newY);
            wasInserted = true;
            break;
        }
    }

    limit = calArray->calPointCnt;
    if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
    for (index = 0; index < limit  &&  !wasInserted; index++)
    {
        if (calArray->calPoint[index].Y > newY)
        {
            /*  Move everything down and add this new point  */
            moveCalPointsDown(sensor, index);

            if (shouldShowCsvForDebug())
            {
                dbgSerial.print(F("Inserting at ["));
                dbgSerial.print(index);
                dbgSerial.print(F("]  X="));
                dbgSerial.print(newX);
                dbgSerial.print(F("  tare="));
                dbgSerial.print(tareOffset[sensor]);
                dbgSerial.print(F("  Y="));
                dbgSerial.print(newY);
                dbgSerial.print(F("  PointCnt="));
                dbgSerial.println(calArray->calPointCnt);
            }

            calArray->calPoint[index].X = getLastFilteredSensorReading(sensor) - tareOffset[sensor];
            calArray->calPoint[index].Y = newY;

            if (calArray->calPoint[index].Y == 0)
            {
                //  This is setting a new tare point, make sure both X and Y are zero.
                calArray->calPoint[index].X = 0;
            }
            sendEvent(msmtMgrEvt_ReportCalibrationSet1, sensor, newX);
            sendEvent(msmtMgrEvt_ReportCalibrationSet2, sensor, newY);
            wasInserted = true;
        }
    }

    if (!wasInserted  &&  calArray->calPointCnt < MAX_CAL_POINTS_PER_SENSOR)
    {
        if (shouldShowCsvForDebug())
        {
            dbgSerial.println("Inserting on the end");
        }
        calArray->calPoint[calArray->calPointCnt].X = getLastFilteredSensorReading(sensor) - tareOffset[sensor];
        calArray->calPoint[calArray->calPointCnt].Y = newY;
        if (calArray->calPoint[calArray->calPointCnt].Y == 0)
        {
            //  This is setting a new tare point, make sure both X and Y are zero.
            calArray->calPoint[calArray->calPointCnt].X = 0;
        }
        sendEvent(msmtMgrEvt_ReportCalibrationSet1, sensor, newX);
        sendEvent(msmtMgrEvt_ReportCalibrationSet2, sensor, newY);
        wasInserted = true;
        calArray->calPointCnt++;
    }

#ifdef  SHOW_CAL_DEBUG
    printCalArray(sensor);
#endif //  SHOW_CAL_DEBUG

    if (wasInserted)
    {
        writeCalDataToFlash();
//      dbgSerial.println(F("Wrote calData to Flash"));
    }
    checkCalTablesForFunnyBusiness(__LINE__);
}


/*  Remove a cal point by searching for the X value.  Other values are
    moved up.

    If there is no match, then nothing changes.
*/
void deleteCalPointWithX(sensorType_t sensor, int32_t oldX)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];
    int16_t             index;

    int   limit = calArray->calPointCnt;
    if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
    for (index = 0; index < limit; index++)
    {
        if (oldX == calArray->calPoint[index].X)
        {
            moveCalPointsUp(sensor, index);
            break;
        }
    }
    writeCalDataToFlash();
}


/*  Remove a cal point at a specific index.  Other values are
    moved up.
*/
void deleteCalPointWithIndex(sensorType_t sensor, int16_t index)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("\ndeleteCalPointWithIndex entry  Sensor="));
    dbgSerial.print(sensor);
    dbgSerial.print(F("   index="));
    dbgSerial.println(index);
    dbgSerial.println();
    printCalArray(sensor);
#endif

    checkCalTablesForFunnyBusiness(__LINE__);
    if (sensor >= 0  &&  sensor < sensorType_MaxSensors  &&  index >= 0  && index < MAX_CAL_POINTS_PER_SENSOR)
    {
        moveCalPointsUp(sensor, index);
    }
    checkCalTablesForFunnyBusiness(__LINE__);

    if (sensor == 100  &&  index == 100)
    {
        dbgSerial.println(F("delete all calibration points"));
//#ifdef  SHOW_CAL_DEBUG
        dbgSerial.println(F("delete all calibration points"));
//#endif
        //  This is a special code that deletes ALL cal points on ALL sensors
        for (int index = 0; index < sensorType_MaxSensors; index++)
        {
            deleteAllCalibration(index);
        }
    }
    else if (sensor >= 0  &&  sensor <= sensorType_MaxSensors  &&  index == 100)
    {
//      dbgSerial.print(F("delete single calibration point sensor="));
//      dbgSerial.println(sensor);

        //  This is a special code that deletes ALL cal points on a single sensor
        deleteAllCalibration(sensor);
    }

    checkCalTablesForFunnyBusiness(__LINE__);

    writeCalDataToFlash();

#ifdef  SHOW_CAL_DEBUG
#endif  //  SHOW_CAL_DEBUG
    checkCalTablesForFunnyBusiness(__LINE__);
}


/*  Delete all calibration for a sensor  */
void deleteAllCalibration(sensorType_t sensor)
{
#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("deleteAllCalibration entry  Sensor="));
    dbgSerial.println(sensor);
#endif  //  SHOW_CAL_DEBUG
    checkCalTablesForFunnyBusiness(__LINE__);
    calPointArray[sensor].calPointCnt = 0;
    checkCalTablesForFunnyBusiness(__LINE__);
    writeCalDataToFlash();
}


/*  Search a calibration table and find the interpolated value.

    If the value of X is beyond the table edge, use the closest
    point and project the line using them.

    If there are less than two calibration points, return 0x8000.

    X is ADC counts, a signed 32-bit number
*/
int16_t findYFromCalibration(sensorType_t sensor, int32_t incomingX)
{
    pCalPointArray_t    calArray = &calPointArray[sensor];
    int16_t             index    = -99;     //  Bogus value, so we know if has been assigned
    bool                xValuesIncrementInCal;
    int32_t             thisTareOffset = getCalTarePoint(sensor);
    int                 limit = calArray->calPointCnt;

#if  defined(SHOW_CAL_DEBUG) &&  defined(NEVER_DO_THIS)
    if (sensor == sensorType_Test)
    {
    dbgSerial.print(F("FindYFromCalibration()   sensor="));
    dbgSerial.print(sensor);
    dbgSerial.print(F("   X to find="));
    dbgSerial.println(incomingX);
    }
#endif  //  SHOW_CAL_DEBUG
    checkCalTablesForFunnyBusiness(__LINE__);

    if (sensor == sensorType_Test)
    {
        printf("");
    }


#ifdef  SHOW_CAL_DEBUG
    if (sensor == sensorType_Test)
    {
        enableCalibrationCalcPrinting();
    }
    else
#endif  //  SHOW_CAL_DEBUG
    {
        disableCalibrationCalcPrinting();
    }

    checkCalTablesForFunnyBusiness(__LINE__);

    if (calArray->calPointCnt < 2)
    {
//DMMDBG
//#define SHOW_FINDY_DEBUG
#ifdef  SHOW_FINDY_DEBUG
        if (showCalibrationCalcSteps  &&  sensor == sensorType_Test)
        {
            dbgSerial.print(F("FindY:not enough cal data.   Cnt="));
            dbgSerial.print(calArray->calPointCnt);
            dbgSerial.print(F("   sensor="));
            dbgSerial.println(sensor);
        }
#endif  SHOW_FINDY_DEBUG
        //  Not enough calibration data, return default value
        return -32768;
    }
    else
    {
//DMMDBG

#ifdef  SHOW_FINDY_DEBUG
        if (showCalibrationCalcSteps  &&  sensor == sensorType_Test)
        {
            dbgSerial.print(F("FindY:Enough cal data.   Cnt="));
            dbgSerial.println(calArray->calPointCnt);
            dbgSerial.print(F("   sensor="));
            dbgSerial.println(sensor);

            if (!showCalibrationCalcSteps)
            {
                dbgSerial.println(F("ShowSteps is disabled"));
            }

            printCalArray(sensor);
        }
#endif  SHOW_FINDY_DEBUG
    }

    checkCalTablesForFunnyBusiness(__LINE__);
    /*  We don't know if the X values are ascending or descending.  It all depends
    *  on the gain curve of the sensor.
    */
    limit = calArray->calPointCnt;
    if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
    if (calArray->calPoint[0].X < calArray->calPoint[1].X)
    {
        xValuesIncrementInCal = true;
        if ((incomingX - thisTareOffset) < calArray->calPoint[0].X)
        {
            /*  Below the lower end of the table, just use the first two entries  */
            index = 0;
        }
        else if ((incomingX - thisTareOffset) > calArray->calPoint[limit - 1].X)
        {
            /*  Above the upper end of the table, use the last two entries  */
            index = calArray->calPointCnt - 2;
        }
    }
    else
    {
        xValuesIncrementInCal = false;
        if ((incomingX - thisTareOffset) < calArray->calPoint[limit - 1].X)
        {
            /*  Above the upper end of the table, use the last two entries  */
            index = calArray->calPointCnt - 2;
        }
        else if ((incomingX - thisTareOffset) > calArray->calPoint[0].X)
        {
            /*  Below the lower end of the table, just use the first two entries  */
            index = 0;
        }
    }

    //  If index is still negative, we are in the cal table somewhere and need to find the corresponding entries
    if (index < 0)
    {
        for (index = 0; index <= calArray->calPointCnt - 1; index++)
        {
            if (xValuesIncrementInCal)
            {
                if ((incomingX - thisTareOffset) <= calArray->calPoint[index].X)
                {
                    /*  We found the range for positive sorting, we went one too far  */
                    index--;
                    break;
                }
            }
            else
            {
                if ((incomingX - thisTareOffset) >= calArray->calPoint[index].X)
                {
                    /*  We found the range for negative sorting  */
                    index--;
                    break;
                }
            }
        }
    }

    limit = calArray->calPointCnt;
    if (limit > MAX_CAL_POINTS_PER_SENSOR) limit = MAX_CAL_POINTS_PER_SENSOR;
    if (index > (limit - 2))
    {
        /*  Off the end of the table, use the last two entries  */
        index = limit - 2;
    }
    if (index < 0)
    {
        /*  Below the first entry in the table, use the first two entries  */
        index = 0;
    }

    checkCalTablesForFunnyBusiness(__LINE__);

#ifdef  SHOW_CAL_DEBUG
//  dbgSerial.print(F("Starting computations   ShowCal="));
//  dbgSerial.print(showCalibrationCalcSteps);
//  dbgSerial.print(F("  sensor="));
//  dbgSerial.println(sensor);
#endif  //  SHOW_CAL_DEBUG

    if (sensor == sensorType_DistanceLength)
    {
        showCalibrationCalcSteps = true;
    }

//  DMMDBG
//#define SHOW_FINDY_DEBUG
#ifdef  SHOW_FINDY_DEBUG
    if (shouldShowCsvForDebug()  &&  showCalibrationCalcSteps  &&  sensor == sensorType_Test)
    {
        printCalArray(sensor);
        dbgSerial.print(F("FindY:Using Index "));
        dbgSerial.print(index);
        dbgSerial.print(F("  direction="));
        dbgSerial.print(xValuesIncrementInCal ? 1 : -1);
        dbgSerial.print(F("  X="));
        dbgSerial.print(incomingX);
        dbgSerial.print(F("    tareOffset="));
        dbgSerial.print(thisTareOffset);
        dbgSerial.println();
    }
    else
    {
//      dbgSerial.println(F("NoCalToShow"));
    }
#endif //  SHOW_FINDY_DEBUG

    /*  At this point, index holds the first of two entries that bracket X.  Use
        the two entries to find slope and Yintercept, then apply to the incoming
        value to compute the new Result.
    */
    {
        /*  Everything is done in float values  */
        float       lowX  = (float)(calArray->calPoint[index + 0].X),
                    lowY  = (float) calArray->calPoint[index + 0].Y,
                    highX = (float)(calArray->calPoint[index + 1].X),
                    highY = (float) calArray->calPoint[index + 1].Y;

        /*  Difference in Y is in millimeters/grams, so the biggest it can be is about 1000.
            Work is done in floating point, so we cover the wide range of values
            at the cost of some low end precision.
        */
        float       slope       = ((float)highY - (float)lowY) / ((float)highX - (float)lowX);
        float       intercept   = (float)lowY - slope * (float)lowX;
        int16_t     finalAnswer = (int16_t)(slope * (float)(incomingX - thisTareOffset) + intercept + 0.5);

        calSlopeSignIsPositive[sensor] = (slope >= 0.0) ? true : false;

        //  Remember the intermediate values, we need them later when computing density directly
        currentMXplusB_Slope     [sensor] = slope;
        currentMXplusB_Intercept [sensor] = intercept;
        currentMXplusB_tareOffset[sensor] = thisTareOffset;

//#define SHOW_FINDY_DEBUG
#ifdef  SHOW_FINDY_DEBUG
        displaySlopeAndIntercept = true;
#endif  //  SHOW_FINDY_DEBUG
        if (displaySlopeAndIntercept  &&  showCalibrationCalcSteps)
        {
            displaySlopeAndIntercept = false;
            {
                dbgSerial.print(F(" LowX : "));
                dbgSerial.print((int32_t)lowX);
                dbgSerial.print(F(" LowY : "));
                dbgSerial.print((int32_t)lowY);
                dbgSerial.print(F(" HighX : "));
                dbgSerial.print((int32_t)highX);
                dbgSerial.print(F(" HighY : "));
                dbgSerial.print((int32_t)highY);
                dbgSerial.print(F("  X="));
                dbgSerial.print(incomingX);
                dbgSerial.print(F("  taredX="));
                dbgSerial.print(incomingX - thisTareOffset);
                dbgSerial.print(F(" Slope="));
                dbgSerial.print((double)slope, 8);
                dbgSerial.print(F(" Intercept="));
                dbgSerial.println((int16_t)intercept);
                dbgSerial.print(F("\nAnswer="));
                dbgSerial.print(finalAnswer);
                dbgSerial.println();
            }
        }

        if (sensor == sensorType_DistanceLength)
        {
            showCalibrationCalcSteps = true;
        }


        /*  Return the new Y value (as an integer)  */
        return finalAnswer;
    }

    disableCalibrationCalcPrinting();   //  DMMDBG - For debug

    checkCalTablesForFunnyBusiness(__LINE__);

    return 0x7FFF;    //  Max value for int16_t
}


/*  Return the current X value for the sensor at cal index.

    returns 0 if there is no value.
*/
int32_t getCalPoint_X_byIndex(sensorType_t sensor, int16_t index)
{
    int32_t returnValue = 0;

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("getCalPoint_X_byIndex()  sensor="));
    dbgSerial.print(sensor);
    dbgSerial.print(F("  index="));
    dbgSerial.print(index);
    dbgSerial.print(F("  startingX="));
    dbgSerial.print(calPointArray[sensor].calPoint[index].X);
#endif  //  SHOW_CAL_DEBUG

    if (sensor < 0  ||  sensor >= sensorType_MaxSensors  ||  index < 0  || index >= MAX_CAL_POINTS_PER_SENSOR)
    {
        //  Invalid inputs, just ignore all
        return returnValue;
    }

    if (index < calPointArray[sensor].calPointCnt)
    {
        returnValue = calPointArray[sensor].calPoint[index].X;
    }

#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("  returnValue="));
    dbgSerial.print(returnValue);
    dbgSerial.println();
#endif  //  SHOW_CAL_DEBUG

    return returnValue;
}


/*  Return the current Y value for the sensor at cal index.

    returns 0 if there is no value.
*/
int32_t getCalPoint_Y_byIndex(sensorType_t sensor, int16_t index)
{
    int32_t returnValue = -99;

#ifdef  SHOW_CAL_DEBUG
        dbgSerial.print(F("getCalPoint_Y_byIndex(sensor="));
        dbgSerial.print(sensor);
        dbgSerial.print(F(",  index="));
        dbgSerial.print(index);
        dbgSerial.println(F(")"));
#endif  //  SHOW_CAL_DEBUG

    if (sensor >= 0  &&   sensor < sensorType_MaxSensors  &&
        index  >= 0  &&    index < calPointArray[sensor].calPointCnt)
    {
        returnValue = calPointArray[sensor].calPoint[index].Y;
    }

    return returnValue;
}

/*  Adjust an entire calibration table by setting the tare offset
 *  for the sensor.  The tare offset applies to all calibration  points.
 */
void addCalOffset(sensorType_t sensor, int32_t newTarePoint)
{
#ifdef  SHOW_CAL_DEBUG
    dbgSerial.print(F("addCalOffset() entry   sensor="));
    dbgSerial.println(sensor);
    dbgSerial.println(F("*************************************************"));
#endif  //  SHOW_CAL_DEBUG

    if (sensor >= 0  &&  sensor < sensorType_MaxSensors)
    {
        tareOffset[sensor] = newTarePoint;
    }
}


bool setCurrentTarePoint(sensorType_t sensor, int32_t newTare)
{
//  #ifdef  SHOW_CAL_DEBUG
    if (0  &&  sensor == sensorType_Test)
    {
        dbgSerial.print(F("Setting tare   sensor="));
        dbgSerial.println(sensor);
    }
//  #endif  //  SHOW_CAL_DEBUG

    checkCalTablesForFunnyBusiness(__LINE__);

    if (1  &&  sensor == sensorType_Test)
    {
        printf("");
    }

    if (sensor >= 0  &&  sensor < sensorType_MaxSensors)
    {
        if (((newTare < -7500000  ||  newTare >  7500000)  &&  sensor != sensorType_DistanceLength)  ||
            ((newTare < -8300000  ||  newTare >  8300000)  &&  sensor == sensorType_DistanceLength))
        {
            return false;
        }

        if (newTare == 0)
        {
            tareOffset[sensor] = getLastFilteredSensorReading(sensor);
        }
        else
        {
            tareOffset[sensor] = newTare;
        }

        //  Determine if we have a cal point with Y=0.  If not, add it
        calPoint_t    *zeroPointPtr = &calPointArray[sensor].calPoint[0];
        bool          zeroPointWasFound = false;
        int           index;

        for (index = 0; index < calPointArray[sensor].calPointCnt; index++)
        {
            if (zeroPointPtr->Y == 0)
            {
                zeroPointWasFound = true;
                break;
            }
            zeroPointPtr++;
        }
        if (!zeroPointWasFound)
        {
            addCalPoint(sensor, tareOffset[sensor], 0);
        }
        if (sensor == sensorType_Test)
        {
            setTareMark(sensor);
        }

        if (tareOffset[sensor] > 3290000)
        {
            printf("");
        }
        return true;
    }
    checkCalTablesForFunnyBusiness(__LINE__);
    return false;
}


/*  Find and return the index for a specific value of Y.  If the item is
 *  not found, this function returns -1.
 */
int16_t getCalIndexOfY(sensorType_t sensor, int16_t yToFind)
{
    int16_t returnValue = -1;

    if (sensor < 0  ||  sensor >= sensorType_MaxSensors  ||  yToFind < 0  || yToFind >= 2000)
    {
        //  Invalid inputs, just ignore all
        return returnValue;
    }

    for (int16_t index = 0; index < calPointArray[sensor].calPointCnt; index++)
    {
        if (calPointArray[sensor].calPoint[index].Y == yToFind)
        {
            returnValue = index;
            break;
        }
    }
    return returnValue;
}


static void updateSingleEepromByte(int addr, uint8_t newValue)
{
    if (newValue == EEPROM.read(addr))
    {   //  Same value, nothing to do
#ifdef  DONT_DO
        dbgSerial.print(F("wrote same byte #"));
        dbgSerial.print(addr);
        dbgSerial.print(F("  val="));
        dbgSerial.print(newValue);
        dbgSerial.println();
#endif  //  DONT_DO
        return;
    }

    for (int index = 0; index < 8; index++)
    {
        EEPROM.write(addr, newValue);

        if (newValue == EEPROM.read(addr))
        {
#ifdef  DONT_DO
            dbgSerial.print(F("wrote good byte #"));
            dbgSerial.print(addr);
            dbgSerial.print(F("  val="));
            dbgSerial.print(newValue);
            dbgSerial.print(F("  tries="));
            dbgSerial.print(index + 1);
            dbgSerial.println();
#endif  //    DONT_DO
            break;
        }
#ifdef  DONT_DO
        dbgSerial.println(F("retry updating EEPROM value"));
#endif  //  DONT_DO
    }
}


void writeCalDataToFlash(void)
{
//  EEPROM.put(0, calPointArray);

    uint8_t   *srcPtr;
    uint16_t  bytesRemaining;
    bool      writeError = false;
    int       srcCtr;
    uint8_t   readbackByte;
    int       calCount = sizeof(calPointArray);

#ifdef  DONT_DO
    dbgSerial.println(F("writeCalDataToFlash entry"));
#endif  //  DONT_DO

    delay(5);     //  Start out with a bit of delay, just because...

    srcCtr         = 0;
    srcPtr         = (uint8_t *)calPointArray;
    bytesRemaining = sizeof(calPointArray)  //  Calibration array
                            + 2             //  EEPROM version storage
                            + 100           //  Padding
                            + 4;            //  Remaining stick count

#ifdef  DONT_DO
    dbgSerial.print(F("  RAMaddr=0x"));
    dbgSerial.print((long)srcPtr, HEX);
    dbgSerial.println();
#endif  //  DONT_DO

    updateSingleEepromByte(srcCtr++, CALIBRATION_VERSION);      //  Write the first version number
    bytesRemaining--;

    while (calCount-- > 1)
    {
        updateSingleEepromByte(srcCtr++, *srcPtr++);
        bytesRemaining--;
    }
    updateSingleEepromByte(srcCtr++, CALIBRATION_VERSION+1);      //  Write the second version number
    bytesRemaining--;


    //  Add 100 bytes of padding for the future
    bytesRemaining -= 100;
    srcCtr += 100;


    //  Write 4 bytes of sticksRemaining,big endian style
    int32_t     sticksRemaining = tallyTrkObj_GetStickCount();

    updateSingleEepromByte(srcCtr++, 0xFF & (sticksRemaining >> 24));
    updateSingleEepromByte(srcCtr++, 0xFF & (sticksRemaining >> 16));
    updateSingleEepromByte(srcCtr++, 0xFF & (sticksRemaining >>  8));
    updateSingleEepromByte(srcCtr++, 0xFF & (sticksRemaining >>  0));

    //  Write accumulation statistics like BFT, total volume, etc
    int16_t      accumDataSize;

    srcPtr = (uint8_t *)getAccumulationDataStructure(&accumDataSize);
    while (accumDataSize-- > 1)
    {
        updateSingleEepromByte(srcCtr++, *srcPtr++);
        accumDataSize--;
    }

    //  Now reset and confirm what we just wrote
    srcCtr         = 0;
    srcPtr         = (uint8_t *)calPointArray;
    bytesRemaining = sizeof(calPointArray)  //  Calibration array
                            + 2             //  EEPROM version storage
                            + 100           //  Padding
                            + 4;            //  Remaining stick count

#ifdef  DONT_DO
    dbgSerial.print(F("  RAMaddr=0x"));
    dbgSerial.print((long)srcPtr, HEX);
    dbgSerial.println();
#endif  //  DONT_DO

    if (EEPROM.read(srcCtr++) != CALIBRATION_VERSION)
    {
        writeError = true;
#ifdef  DONT_DO
        dbgSerial.println(F("Error in first version readback"));
#endif  //  DONT_DO
    }
    bytesRemaining--;
    while (bytesRemaining-- > 1)
    {
        readbackByte = EEPROM.read(srcCtr);

        if (readbackByte != *srcPtr)
        {
            writeError = true;
#ifdef  DONT_DO
            dbgSerial.print(F("Error readback error in byte#"));
            dbgSerial.print(srcCtr);
            dbgSerial.print(F("  RAM="));
            dbgSerial.print(*srcPtr & 0xFF);
            dbgSerial.print(F("  EEPROM="));
            dbgSerial.println(readbackByte);
#endif  //  DONT_DO
        }
        srcCtr++;
        srcPtr++;
    }
    if (EEPROM.read(srcCtr++) != CALIBRATION_VERSION+1)
    {
        writeError = true;
#ifdef  DONT_DO
        dbgSerial.println(F("Error in second version readback"));
#endif  //  DONT_DO
    }
    bytesRemaining--;

    bytesRemaining -= 100;  //  Skip the reserved byte area
    srcCtr += 100;

    //  Write 4 bytes of sticksRemaining,big endian style
    int32_t     checkSticksRemaining = tallyTrkObj_GetStickCount();
    if (EEPROM.read(srcCtr++) != (0xFF & (checkSticksRemaining >> 24))) writeError = true;
    if (EEPROM.read(srcCtr++) != (0xFF & (checkSticksRemaining >> 16))) writeError = true;
    if (EEPROM.read(srcCtr++) != (0xFF & (checkSticksRemaining >>  8))) writeError = true;
    if (EEPROM.read(srcCtr++) != (0xFF & (checkSticksRemaining >>  0))) writeError = true;
    bytesRemaining -= 4;
    srcCtr += 4;

    srcPtr = (uint8_t *)getAccumulationDataStructure(&accumDataSize);
    while (accumDataSize-- > 1)
    {
        if (EEPROM.read(srcCtr++) != *srcPtr++) writeError = true;

        updateSingleEepromByte(srcCtr++, *srcPtr++);
    }

#ifdef  DONT_DO
    dbgSerial.println(F("writeCalDataToFlash exit"));
#endif  //  DONT_DO
}


void readCalDataFromFlash(bool setStickcount)
{
//  EEPROM.get(0, calPointArray);

    uint8_t  *destPtr = (uint8_t *)calPointArray;
    uint16_t  srcCtr = 0;
    uint16_t      bytesRemaining = sizeof(calPointArray)  //  Calibration array
                                   + 2                    //  EEPROM version storage
                                   + 100                  //  Padding
                                   + 4;                   //  Remaining stick count

    int       firstCalVersion  = 0xFFF,
              secondCalVersion = 0xFFF,
              calCount         = sizeof(calPointArray);

//#define EEPROM_DEBUG
#ifdef  EEPROM_DEBUG
    dbgSerial.print(F("  entry"));
    dbgSerial.print(F("EEPROM bytes="));
    dbgSerial.println(bytesRemaining);
#endif

    firstCalVersion = EEPROM.read(srcCtr++);
    bytesRemaining--;
    while (calCount-- > 1)
    {
        *destPtr++ = EEPROM.read(srcCtr++);
        bytesRemaining--;
    }
    secondCalVersion = EEPROM.read(srcCtr++);
    bytesRemaining--;

#ifdef  DONT_DO
    dbgSerial.print(F("checking readback  1st="));
    dbgSerial.print(firstCalVersion);
    dbgSerial.print(F("  2nd="));
    dbgSerial.print(secondCalVersion);
    dbgSerial.print(F("  ideal="));
    dbgSerial.print(CALIBRATION_VERSION);
    dbgSerial.println();
#endif  //  DONT_DO

    if (firstCalVersion != CALIBRATION_VERSION  ||  secondCalVersion != CALIBRATION_VERSION+1)
    {
        //  Version numbers don't match up, kill all calibration data
        dbgSerial.println(F("VersionNum wrong, killing calibration"));
        for (int sensorId = 0; sensorId < sensorType_MaxSensors; sensorId++)
        {
            calPointArray[sensorId].calPointCnt = 0;
            for (int calCount = 0; calCount < MAX_CAL_POINTS_PER_SENSOR; calCount++)
            {
                calPointArray[sensorId].calPoint[calCount].X = 0;
                calPointArray[sensorId].calPoint[calCount].Y = 0;
            }
        }
        dbgSerial.println(F("VersionNum wrong, writing back"));
        writeCalDataToFlash();      //  Put it all back into EEPROM
#ifdef  DONT_DO
        dbgSerial.println(F("VersionNum wrong, done writing back"));
#endif  //  DONT_DO
        return;
    }

    for (int index = 0; index < sensorType_MaxSensors; index++)
    {
        if (calPointArray[index].calPointCnt > MAX_CAL_POINTS_PER_SENSOR  ||  calPointArray[index].calPointCnt < 0)
        {
#ifdef  DONT_DO
            dbgSerial.print(index);
            dbgSerial.print(F("  count="));
            dbgSerial.println(calPointArray[index].calPointCnt);
#endif  //  DONT_DO
            calPointArray[index].calPointCnt = 0;
            writeCalDataToFlash();      //  Put it all back into EEPROM
        }
    }

    checkCalTablesForFunnyBusiness(__LINE__);
#ifdef  DONT_DO
    dbgSerial.println(F("  exit"));
#endif  //  DONT_DO

    srcCtr += 100;
    bytesRemaining -= 100;

    uint32_t    newRemainingSticks = 0;

    newRemainingSticks |= ((uint32_t)EEPROM.read(srcCtr++) << 24);
    newRemainingSticks |= ((uint32_t)EEPROM.read(srcCtr++) << 16);
    newRemainingSticks |= ((uint32_t)EEPROM.read(srcCtr++) <<  8);
    newRemainingSticks |= ((uint32_t)EEPROM.read(srcCtr++) <<  0);
    if (setStickcount)
    {
        sendEvent(tallyTrkEvt_SetCountRemaining, newRemainingSticks, 0);
    }
    bytesRemaining -= 4;

    int16_t   accumDataSize;
    destPtr = (uint8_t *)getAccumulationDataStructure(&accumDataSize);
    while (accumDataSize-- > 1)
    {
        *destPtr++ = EEPROM.read(srcCtr++);
    }
}

float   theRealDenominator;

/*  Compute density from raw ADC counts before applying any scaling or rounding,
    then round as the very last step.  This avoids trunction and rounding errors
    in each of the four measurements from creeping in early.
*/
float densityFromRawmsmts_kgm3(void)
{
    /*  We use the following:
        1)  Raw measurements
        2)  Intermediate values (slope, intercept, tare offset)
    */

    float   returnValue,
            myValues_ADC[sensorType_MaxSensors];

    for (int sensorId = 0; sensorId < sensorType_MaxSensors; sensorId++)
    {
        int32_t     lastSensorReading =  getLastFilteredSensorReading(sensorId);
        int16_t     unusedReturnValue;

        //  Force update of MXplusB values to make sure we have the lastest
        //  This is probably useless and wastes time.
        unusedReturnValue = findYFromCalibration(sensorId, lastSensorReading);

        //  First off, get raw values and remove the tare offset, convert to float
        myValues_ADC[sensorId] = (float)(lastSensorReading - currentMXplusB_tareOffset[sensorId]);

        //  Remove the scaled intercept portion.  We need values that are referenced to the
        //  origin (ie - intercept is zero) and only need a scaling factor applied to make mm/grams.
        myValues_ADC[sensorId] = myValues_ADC[sensorId] + currentMXplusB_Intercept[sensorId] / currentMXplusB_Slope[sensorId];
    }

#ifdef  FORCED_HEIGHT_VALUE
    myValues_ADC        [sensorType_DistanceHeight] = 1;
    currentMXplusB_Slope[sensorType_DistanceHeight] = 1;
#endif  //  FORCED_HEIGHT_VALUE

#ifdef  FORCED_WEIGHT_VALUE
    myValues_ADC        [sensorType_Weight] = 1;
    currentMXplusB_Slope[sensorType_Weight] = 1;
#endif  //  FORCED_WEIGHT_VALUE


    //  Construct the density from the four measurements. At this point,
    //  All the offsets have been removed from the measurements

    theRealDenominator = myValues_ADC[sensorType_DistanceLength] * myValues_ADC[sensorType_DistanceWidth] * myValues_ADC[sensorType_DistanceHeight];

    //  Compute the density in unscaled ADC counts
    returnValue = myValues_ADC[sensorType_Weight] / theRealDenominator;

#if    defined(_WIN32)
    debugWeight = myValues_ADC[sensorType_Weight        ] * currentMXplusB_Slope[sensorType_Weight        ];
    debugHeight = myValues_ADC[sensorType_DistanceHeight] * currentMXplusB_Slope[sensorType_DistanceHeight];
    debugLength = myValues_ADC[sensorType_DistanceLength] * currentMXplusB_Slope[sensorType_DistanceLength];
    debugWidth  = myValues_ADC[sensorType_DistanceWidth ] * currentMXplusB_Slope[sensorType_DistanceWidth ];
    printf("'%12d", eventTimestamp);
    printf("   weight=%0.5fg",  (float)debugWeight);
    printf("   height=%0.5fmm", (float)debugHeight);
    printf("   length=%0.5fmm", (float)debugLength);
    printf("   width =%0.5fmm", (float)debugWidth );
    printf("   denominator=%12.5f", theRealDenominator);
    printf("\n");
    printf("");
#endif //   defined(_WIN32) //  Apply all the scaling factors to convert to grams/mm3 returnValue *= currentMXplusB_Slope[sensorType_Weight] / (currentMXplusB_Slope[sensorType_DistanceLength] * currentMXplusB_Slope[sensorType_DistanceWidth ] * currentMXplusB_Slope[sensorType_DistanceHeight]);

#ifdef  FORCED_HEIGHT_VALUE
    returnValue /= FORCED_HEIGHT_VALUE;
#endif  //  FORCED_HEIGHT_VALUE

#ifdef  FORCED_WEIGHT_VALUE
    returnValue /= FORCED_WEIGHT_VALUE;
#endif  //  FORCED_WEIGHT_VALUE

    //  Turn it back into density in standard units
    returnValue *=   currentMXplusB_Slope[sensorType_Weight]
                   / currentMXplusB_Slope[sensorType_DistanceLength]
                   / currentMXplusB_Slope[sensorType_DistanceHeight]
                   / currentMXplusB_Slope[sensorType_DistanceWidth ];

    //  Convert grams/mm3 into kg/m3
    returnValue *= 1000000.0;


    return returnValue;
}
