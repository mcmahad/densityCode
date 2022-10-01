

#ifndef   CALIBRATION_H
#define   CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>


#define     MAX_CAL_POINTS_PER_SENSOR       10

typedef     int  sensorType_t;

const       int     sensorType_First = 0,
                    sensorType_DistanceLength = 0,
                    sensorType_DistanceWidth  = 1,       //  3 laser displacement measurements
                    sensorType_DistanceHeight = 2,
                    sensorType_Weight         = 3,       //  1 load cell
                    sensorType_MaxSensors     = 4;




/*  Inserts in order by finding the correct spot and moving everything
    below that down.  Anything on the bottom gets dropped off.

    A matching value of X with an existing cal point will replace that
    point.

    X = ADC counts   Y = millimeters
*/
void addCalPoint(sensorType_t sensor, int32_t newX, int16_t newY);

/*  Debug function to set calibration point data
*/
void forceCalPointPair(sensorType_t sensorIndex, int index, int32_t newX, int16_t newY);


/*  Remove a cal point by searching for the X value.  Other values are
    moved up.

    If there is no match, then nothing changes.
*/
void deleteCalPointWithX(sensorType_t sensor, int32_t oldX);


/*  Remove a cal point at a specific index.  Other values are
    moved up.
*/
void deleteCalPointWithIndex(sensorType_t sensor, int16_t index);

/*  Delete all calibration for a sensor  */
void deleteAllCalibration(sensorType_t sensor);


/*  Search a calibration table and find the interpolated value.

    If the value of X is beyond the table edge, use the closest
    point and project the line using them.

    If there are less than two calibration points, return zero.
*/
int16_t findYFromCalibration(sensorType_t sensor, int32_t incomingX);


/*  Return the current X value for the sensor at cal index.

    returns 0 if there is no value.
*/
int32_t getCalPoint_X_byIndex(sensorType_t sensor, int16_t index);


/*  Return the current Y value for the sensor at cal index.

    returns 0 if there is no value.
*/
int32_t getCalPoint_Y_byIndex(sensorType_t sensor, int16_t index);


/*  Adjust an entire calibration table by an offset that is added
 *   to each X value.  This is part of the tare process when there
 *   are multiple cal points.
 */
void addCalOffset(sensorType_t sensor, int32_t  Xdifference);


/*  Find and return the index for a specific value of Y.  If the item is
 *   not found, this function returns -1.
 */
int16_t getCalIndexOfY(sensorType_t sensor, int16_t yToFind);


/*  Compute density from raw ADC counts before applying any scaling or rounding,
 *  then round as the very last step.  This avoids trunction and rounding errors
 *  in each of the four measurements from creeping in early.
 */
float densityFromRawmsmts_kgm3(void);


/*
 *   Tare a specific sensor by capturing the filtered ADC reading as
 *   the new tare point.  Returns true if the tare point was set.
 */
bool setCurrentTarePoint(sensorType_t sensor, int32_t newTare);

bool validCalExists(sensorType_t sensor);

int checkCalTablesForFunnyBusiness(void);

int32_t getCalTarePoint(sensorType_t sensor);
bool  isCalSlopeSignPositive(sensorType_t sensor);
int32_t getCalAdcCountsPerUnit_x1000(sensorType_t sensor);

/*
 * Store cal data into Flash
 */
void writeCalDataToFlash(void);

/*
 * read cal data from Flash
 */
void readCalDataFromFlash(bool setStickcount);

void printCalArray(sensorType_t sensor);

void enableCalibrationCalcPrinting(void);
void disableCalibrationCalcPrinting(void);

#endif  //  CALIBRATION_H
