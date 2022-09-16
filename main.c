
#ifdef  WIN32

#include <stdio.h>
#include <assert.h>

#include "calibration.h"

int16_t main(int16_t argc, char *argv[])
{
    assert(0 == findYFromCalibration(sensorType_Weight1, 1000000));

    addCalPoint(sensorType_Weight1, 0, 0);
    assert(0 == findYFromCalibration(sensorType_Weight1, 1000000));

    addCalPoint(sensorType_Weight1, 1000000, 1000);
    
    assert( 100   == findYFromCalibration(sensorType_Weight1,   100000));
    assert( 1000  == findYFromCalibration(sensorType_Weight1,  1000000));
    assert( 10000 == findYFromCalibration(sensorType_Weight1, 10000000));

    deleteAllCalibration(sensorType_Weight1);
    assert(0 == findYFromCalibration(sensorType_Weight1, 1000000));

    addCalPoint(sensorType_Weight1, 450000, 300);
    addCalPoint(sensorType_Weight1, 500000, 500);
    addCalPoint(sensorType_Weight1, 400000, 200);
    addCalPoint(sensorType_Weight1, 900000, 900);
    addCalPoint(sensorType_Weight1,      0,   0);
    addCalPoint(sensorType_Weight1, 100000, 100);

    assert( 250  == findYFromCalibration(sensorType_Weight1,  425000));
    assert( 1000 == findYFromCalibration(sensorType_Weight1, 1000000));

    assert(getCalPoint_X_byIndex(sensorType_Weight1, 0) == 0);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 0) == 0);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 1) == 100000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 1) == 100);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 2) == 400000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 2) == 200);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 3) == 450000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 3) == 300);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 4) == 500000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 4) == 500);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 5) == 900000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 5) == 900);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 6) == 0);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 6) == 0);

    deleteCalPointWithIndex(sensorType_Weight1, 3);

    assert(getCalPoint_X_byIndex(sensorType_Weight1, 0) == 0);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 0) == 0);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 1) == 100000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 1) == 100);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 2) == 400000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 2) == 200);
//  assert(getCalPoint_X_byIndex(sensorType_Weight1, 3) == 450000);
//  assert(getCalPoint_Y_byIndex(sensorType_Weight1, 3) == 300);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 3) == 500000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 3) == 500);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 4) == 900000);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 4) == 900);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 5) == 0);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 5) == 0);
    assert(getCalPoint_X_byIndex(sensorType_Weight1, 6) == 0);
    assert(getCalPoint_Y_byIndex(sensorType_Weight1, 6) == 0);

    printf("all done.\n");
    return 0;
}

#endif  //  WIN32
