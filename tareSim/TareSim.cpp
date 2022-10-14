// TareSim.cpp :
//
#ifdef _WIN32

#define             _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "commonHeader.h"
#include "events.h"
#include "calibration.h"
#include "msmtMgrEvtHandler.h"


void readCalDataFromFlash(bool setStickcount);
void enableCalibrationCalcPrinting(void);
void disableCalibrationCalcPrinting(void);

FILE    *inputFilePtr;
int     lineCnt = 0;
char    inputLine[100];

int32_t eventTimestamp;

#ifdef  _WIN32
uint32_t millis(void)
{
    return eventTimestamp;
}
#endif  _WIN32


int getLineCnt(void)
{
    return lineCnt;
}

void processEvents(void);

int main(int argc, char **argv)
{
    bool    done = false;

    inputFilePtr = stdin;

    switch (argc)
    {
    case 3:
        if (_strnicmp("meas", argv[1], 4) == 0)
        {
            showMeasCsv_NotRaw();

            //  Try to open it as a file
            inputFilePtr = fopen(argv[2], "r");
            if (inputFilePtr == NULL)
            {
                fprintf(stderr, "ERROR - Couldn't open source file %s\n", argv[2]);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "ERROR - Unknown option %s\n", argv[1]);
            exit(1);
        }
        break;

    case 2:
        //  First check if it's meas from stdin
        if (strcmp("meas", argv[1]) == 0)
        {
            inputFilePtr = stdin;
            showMeasCsv_NotRaw();
        }
        else
        {
            //  Try to open it as a file
            inputFilePtr = fopen(argv[1], "r");
            if (inputFilePtr == NULL)
            {
                fprintf(stderr, "ERROR - Couldn't open source file %s\n", argv[1]);
                exit(1);
            }
        }
        break;

    default:
    case 1:     //  We take input from stdin
        inputFilePtr = stdin;
        break;
    }


    initializeEvents();

#ifdef DAVE_CAL_VALUES
    setCurrentTarePoint(0, 7054512);            //  Set tare so we can add calibrations
    forceCalPointPair  (0, 0,        0,   0);
    forceCalPointPair  (0, 1, -2917806, 236);
    forceCalPointPair  (0, 2, -3532582, 285);

    setCurrentTarePoint(1, 2975551);   //  Set tare so we can add calibrations
    forceCalPointPair  (1, 0,       0,   0);
    forceCalPointPair  (1, 1, -148086,  88);
    forceCalPointPair  (1, 2, -853283, 121);

    setCurrentTarePoint(2, 4188960);      //  Set tare so we can add calibrations
    forceCalPointPair  (2, 0,       0,   0);

    setCurrentTarePoint(3, 249542);    //  Set tare so we can add calibrations
    forceCalPointPair  (3, 0,     0,   0);
    forceCalPointPair  (3, 1, 14390, 153);
    forceCalPointPair  (3, 2, 33014, 353);
#endif // DAVE_CAL_VALUES

//#define ROSS_20220322_CAL_VALUES
#ifdef ROSS_20220322_CAL_VALUES
    setCurrentTarePoint(0, 2134527);            //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,        0,   0);
    forceCalPointPair(0, 1,   -39882, 103);
    forceCalPointPair(0, 2,  -271898, 657);
    forceCalPointPair(0, 3,  -431161, 1002);

    setCurrentTarePoint(1, 4539777);   //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 2127716, 103);


    setCurrentTarePoint(2, 4578311);      //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 2117623, 103);

    setCurrentTarePoint(3, 7968);    //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 281, 149);
    forceCalPointPair(3, 2, 948, 504);
    forceCalPointPair(3, 3, 1994, 1059);
#endif // DAVE_CAL_VALUES


// #define ROSS_20220324_CAL_VALUES
#ifdef ROSS_20220324_CAL_VALUES
    setCurrentTarePoint(0, 2132127);            //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -26986, 68);
    forceCalPointPair(0, 1, -41472, 103);
    forceCalPointPair(0, 2, -272319, 657);
    forceCalPointPair(0, 3, -431609, 1003);

    setCurrentTarePoint(1, 4536510);   //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 2117623, 103);

    setCurrentTarePoint(2, 4578880);      //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 2117623, 103);

    setCurrentTarePoint(3, 7974);    //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 281, 149);
    forceCalPointPair(3, 2, 948, 504);
    forceCalPointPair(3, 3, 1994, 1059);
#endif // DAVE_CAL_VALUES

//#define ROSS_20220405_CAL_VALUES
#ifdef ROSS_20220405_CAL_VALUES
    setCurrentTarePoint(0, 7034221);            //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,        0,   0);
    forceCalPointPair(0, 1,  -451264, 103); //  Length- the only real change to calibration values
    forceCalPointPair(0, 1, -2887744, 657);

    setCurrentTarePoint(1, 4536510);   //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 2117623, 103);

    setCurrentTarePoint(2, 4578880);      //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 2117623, 103);

    setCurrentTarePoint(3, 7974);    //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 281, 149);
    forceCalPointPair(3, 2, 948, 504);
    forceCalPointPair(3, 3, 1994, 1059);
#endif // DAVE_CAL_VALUES

//#define ROSS_20220410_CAL_VALUES_TABLE1
#ifdef ROSS_20220410_CAL_VALUES_TABLE_1
    //  This is for Table1StickTest
    setCurrentTarePoint(0,          8269428);     //  Set tare so we can add calibrations
    forceCalPointPair  (0, 0,        0,   0);
    forceCalPointPair  (0, 1,  -565230, 103);     //  Length- the only real change to calibration values
    forceCalPointPair  (0, 2, -1779926, 325);
    forceCalPointPair  (0, 3, -3596163, 657);

    setCurrentTarePoint(1, 4489091);              //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);

    setCurrentTarePoint(2, 4525175);              //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 13092);                //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
#endif // DAVE_CAL_VALUES

//#define ROSS_20220410_CAL_VALUES_TABLE2
#ifdef ROSS_20220410_CAL_VALUES_TABLE2
    //  This is for Table2StickTest
    setCurrentTarePoint(0, 2922401);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,       0,   0);
    forceCalPointPair(0, 1, -118266, 103);     //  Length- the only real change to calibration values
    forceCalPointPair(0, 2, -375527, 325);
    forceCalPointPair(0, 3, -757137, 657);

    setCurrentTarePoint(1, 4489091);              //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);

    setCurrentTarePoint(2, 4525175);              //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 13092);                //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
#endif // DAVE_CAL_VALUES


//#define ROSS_20220422_CAL_VALUES_TABLE2
#ifdef  ROSS_20220422_CAL_VALUES_TABLE2
    //  This is for ifmNewTest1-setAspAndAep
    setCurrentTarePoint(0, 2977436);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -1235415, 657);     //  Length

    setCurrentTarePoint(1, 4489091);              //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);

    setCurrentTarePoint(2, 4525175);              //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 2695346);                //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 2268274, 4351);         //  Weight
#endif // DAVE_CAL_VALUES


//#define ROSS_20220503_CAL_VALUES_TEST1
#ifdef  ROSS_20220503_CAL_VALUES_TEST1
    //  This is for "new date SW 19 test 1"
    setCurrentTarePoint(0, 2970996);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,        0,   0);
    forceCalPointPair(0, 1,  -126571,  70);     //  Length
    forceCalPointPair(0, 2,  -190487, 104);
    forceCalPointPair(0, 3,  -602890, 325);
    forceCalPointPair(0, 4,  -819956, 447);
    forceCalPointPair(0, 5, -1125557, 602);
    forceCalPointPair(0, 6, -1235978, 657);

    setCurrentTarePoint(1, 4526047);              //  Set tare so we can add calibrations
    forceCalPointPair(1, 0,       0,   0);
    forceCalPointPair(1, 1,  632392,  32);
    forceCalPointPair(1, 2,  703844,  35);
    forceCalPointPair(1, 3,  945215,  46);
    forceCalPointPair(1, 4, 1340744,  65);
    forceCalPointPair(1, 5, 2129222, 103);

    setCurrentTarePoint(2, 4569994);              //  Set tare so we can add calibrations
    forceCalPointPair(2, 0,       0,   0);
    forceCalPointPair(2, 1,  639163,  32);
    forceCalPointPair(2, 2,  866162,  41);
    forceCalPointPair(2, 3, 1333873,  65);
    forceCalPointPair(2, 4, 1719535,  83);
    forceCalPointPair(2, 5, 2111867, 103);

    setCurrentTarePoint(3, 2617057);            //  Set tare so we can add calibrations
    forceCalPointPair(3, 0,      0,    0);      //  Weight
    forceCalPointPair(3, 1,  20067,   38);
    forceCalPointPair(3, 2,  53555,  102);
    forceCalPointPair(3, 3,  79379,  152);
    forceCalPointPair(3, 4, 164781,  317);
    forceCalPointPair(3, 5, 262149,  504);
    forceCalPointPair(3, 6, 575939, 1109);
#endif // DAVE_CAL_VALUES

// #define ROSS_20220503_CAL_VALUES_TEST2
#ifdef  ROSS_20220503_CAL_VALUES_TEST2
    //  This is for "new date SW 19 test 2"
    setCurrentTarePoint(0, 2970451);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -126571, 70);     //  Length
    forceCalPointPair(0, 2, -190487, 104);
    forceCalPointPair(0, 3, -602890, 325);
    forceCalPointPair(0, 4, -819956, 447);
    forceCalPointPair(0, 5, -1125557, 602);
    forceCalPointPair(0, 6, -1235978, 657);

    setCurrentTarePoint(1, 4525575);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 632392, 32);        //  Width
    forceCalPointPair(1, 2, 703844, 35);
    forceCalPointPair(1, 3, 945215, 46);
    forceCalPointPair(1, 4, 1340744, 65);
    forceCalPointPair(1, 5, 2129222, 103);

    setCurrentTarePoint(2, 4570043);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 639163, 32);
    forceCalPointPair(2, 2, 866162, 41);        //  Height
    forceCalPointPair(2, 3, 1333873, 65);
    forceCalPointPair(2, 4, 1719535, 83);
    forceCalPointPair(2, 5, 2111867, 103);

    setCurrentTarePoint(3, 2615909);            //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 20067, 38);
    forceCalPointPair(3, 2, 53555, 102);        //  Weight
    forceCalPointPair(3, 3, 79379, 152);
    forceCalPointPair(3, 4, 164781, 317);
    forceCalPointPair(3, 5, 262149, 504);
    forceCalPointPair(3, 6, 575939, 1109);
#endif // DAVE_CAL_VALUES

//#define DAVE_RUN_TEST
#ifdef  DAVE_RUN_TEST
    //  This is for "new date SW 19 test 2"
    setCurrentTarePoint(0, 7001042);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,        0,   0);
    forceCalPointPair(0, 1,  -993968,  83);     //  Length
    forceCalPointPair(0, 2, -3478307, 285);

    setCurrentTarePoint(1, 2928493);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0,       0,   0);
    forceCalPointPair(1, 1, -265294,  38);        //  Width
    forceCalPointPair(1, 2, -625024,  88);
    forceCalPointPair(1, 3, -857668, 121);

    setCurrentTarePoint(2, 2011105);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 249187);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0,     0,   0);
    forceCalPointPair(3, 1, 14532, 153);
    forceCalPointPair(3, 2, 33511, 353);        //  Weight
#endif // DAVE_CAL_VALUES

//#define ROSS_20220515_REPEATED_SAME_PIECE
#ifdef  ROSS_20220515_REPEATED_SAME_PIECE

    //  This is for "PieceMrepeated"
    setCurrentTarePoint(0, 7000743);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -993968, 83);     //  Length
    forceCalPointPair(0, 2, -3478307, 285);

    setCurrentTarePoint(1, 2925833);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -265294, 38);        //  Width
    forceCalPointPair(1, 2, -625024, 88);
    forceCalPointPair(1, 3, -857668, 121);

    setCurrentTarePoint(2, 2016350);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 248083);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 14532, 153);
    forceCalPointPair(3, 2, 33511, 353);        //  Weight
#endif  //  ROSS_20220515_REPEATED_SAME_PIECE

//#define DAVE_20220629_REPEATED2_SAME_PIECE
#ifdef  DAVE_20220629_REPEATED2_SAME_PIECE

    //  This is for "pieceM2_repeated.txt"
    setCurrentTarePoint(0, 6989794);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -2922910, 236);     //  Length
    forceCalPointPair(0, 2, -3518441, 285);

    setCurrentTarePoint(1, 2919775);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -263209,  38);        //  Width
    forceCalPointPair(1, 2, -853255, 121);

    setCurrentTarePoint(2, 2016350);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 241616);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 14466, 154);
    forceCalPointPair(3, 2, 33446, 353);        //  Weight
#endif  //  DAVE_20220629_REPEATED2_SAME_PIECE

//#define DAVE_20220629_30Second_REPEATED_SAME_PIECE
#ifdef  DAVE_20220629_30Second_REPEATED_SAME_PIECE

    //  This is for "30SecondOnFastOffLongLog.txt"
    setCurrentTarePoint(0, 6992501);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0,        0,   0);
    forceCalPointPair(0, 1, -2922910, 236);     //  Length
    forceCalPointPair(0, 2, -3486981, 285);

    setCurrentTarePoint(1, 2934550);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -263209,  38);      //  Width
    forceCalPointPair(1, 2, -841204, 121);

    setCurrentTarePoint(2, 2016350);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, 241498);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 14466, 154);
    forceCalPointPair(3, 2, 33538, 353);        //  Weight
#endif  //  DAVE_20220629_30Second_REPEATED_SAME_PIECE

//  #define ROSS_2022_07_14_RossWeightTestOnBrokenSystem
#ifdef  ROSS_2022_07_14_RossWeightTestOnBrokenSystem

    //  This is for "2022-07-14-RossWeightTestOnBrokenSystem.txt"
    setCurrentTarePoint(0, 2962128);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 2, -1185505,  629);

    setCurrentTarePoint(1, 4509995);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 1354142, 63);      //  Width

    setCurrentTarePoint(2, 4554901);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 973034, 45);

    setCurrentTarePoint(3, 0);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
#endif  //  ROSS_2022_07_14_RossWeightTestOnBrokenSystem

//#define DAVE_2022_09_26_WeightStabilitySensor1Only_4th
#ifdef  DAVE_2022_09_26_WeightStabilitySensor1Only_4th

    //  This is for "WeightStabilitySensor1Only-4th.log"
    setCurrentTarePoint(0, 8101602);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -592717, 48);
    forceCalPointPair(0, 2, -3515794, 285);

    setCurrentTarePoint(1, 3666459);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -345946,  48);      //  Width
    forceCalPointPair(1, 2, -868033, 122);      //  Width

    setCurrentTarePoint(2, 4554901);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, -11627);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 8630, 77);
    forceCalPointPair(3, 2, 39986, 351);
#endif  //  DAVE_2022_09_26_WeightStabilitySensor1Only_4th


// #define DAVE_2022_10_02_LengthChange_About14mm_Length0
#ifdef  DAVE_2022_10_02_LengthChange_About14mm_Length0

    //  This is for "DAVE_2022_10_02_LengthChange_About14mm_Length0.log"
    setCurrentTarePoint(0, 5608299);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -592717, 48);
    forceCalPointPair(0, 2, -3515794, 285);

    setCurrentTarePoint(1, 3666459);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -345946, 48);       //  Width
    forceCalPointPair(1, 2, -868033, 122);      //  Width

    setCurrentTarePoint(2, 4554901);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, -11627);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 8630, 77);
    forceCalPointPair(3, 2, 39986, 351);
#endif  //  DAVE_2022_10_02_LengthChange_About14mm_Length0

//#define  // #define DAVE_2022_10_02_LengthChange_About14mm_Length0
#ifdef  DAVE_2022_10_02_LengthChange_About14mm_Length0

    //  This is for "DAVE_2022_10_02_LengthChange_About14mm_Length0.log"
    setCurrentTarePoint(0, 5608299);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -592717, 48);
    forceCalPointPair(0, 2, -3515794, 285);

    setCurrentTarePoint(1, 3666459);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, -345946, 48);       //  Width
    forceCalPointPair(1, 2, -868033, 122);      //  Width

    setCurrentTarePoint(2, 4554901);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);

    setCurrentTarePoint(3, -11627);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 8630, 77);
    forceCalPointPair(3, 2, 39986, 351);
#endif  //  DAVE_2022_10_02_LengthChange_About14mm_Length0


#define ROSS_2022_10_05_81grms
#ifdef  ROSS_2022_10_05_81grms

    //  This is for "81grms.log"
    setCurrentTarePoint(0, 5051252);     //  Set tare so we can add calibrations
    forceCalPointPair(0, 0, 0, 0);
    forceCalPointPair(0, 1, -192377, 86);
    forceCalPointPair(0, 2, -435770, 198);
    forceCalPointPair(0, 3, -612821, 278);
    forceCalPointPair(0, 4, -691962, 311);
    forceCalPointPair(0, 5, -771942, 347);
    forceCalPointPair(0, 6, -946564, 425);
    forceCalPointPair(0, 7, -1209731, 538);
    forceCalPointPair(0, 8, -1415604, 629);

    setCurrentTarePoint(1, 3946524);            //  Set tare so we can add calibrations
    forceCalPointPair(1, 0, 0, 0);
    forceCalPointPair(1, 1, 543602, 30);
    forceCalPointPair(1, 2, 707116, 38);
    forceCalPointPair(1, 3, 730990, 40);
    forceCalPointPair(1, 4, 935281, 50);
    forceCalPointPair(1, 5, 1180994, 63);
    forceCalPointPair(1, 6, 1297519, 70);
    forceCalPointPair(1, 7, 1337600, 73);
    forceCalPointPair(1, 8, 1786093, 100);

    setCurrentTarePoint(2, 4721057);            //  Set tare so we can add calibrations
    forceCalPointPair(2, 0, 0, 0);
    forceCalPointPair(2, 1, 658580, 30);
    forceCalPointPair(2, 2, 805011, 38);
    forceCalPointPair(2, 3, 839150, 40);
    forceCalPointPair(2, 4, 1105888, 50);
    forceCalPointPair(2, 5, 1313050, 63);
    forceCalPointPair(2, 6, 1456398, 70);
    forceCalPointPair(2, 7, 1516469, 73);
    forceCalPointPair(2, 8, 2100871, 100);

    setCurrentTarePoint(3, 74476);             //  Set tare so we can add calibrations
    forceCalPointPair(3, 0, 0, 0);
    forceCalPointPair(3, 1, 5001, 50);
    forceCalPointPair(3, 2, 10057, 100);
    forceCalPointPair(3, 3, 15095, 150);
    forceCalPointPair(3, 4, 20073, 200);
    forceCalPointPair(3, 5, 25089, 250);
    forceCalPointPair(3, 6, 30147, 300);
    forceCalPointPair(3, 7, 35172, 350);
    forceCalPointPair(3, 8, 50259, 500);

#endif  //  DAVE_2022_10_02_LengthChange_About14mm_Length0

    sendEvent(msmtMgrEvt_EnableFinalReports, 0, 0);

    while (!done)
    {
        char        *returnCode;
        int         msgType,
                    data1,
                    data2;

        returnCode = fgets(inputLine, sizeof inputLine, inputFilePtr);
        lineCnt++;
//      printf("lineCnt=%d", lineCnt);

        if (lineCnt == 50  ||  lineCnt == 8740)
        {
            if (0)
            {
                //  Tare during operation after we have some valid data logged
                setCurrentTarePoint(sensorType_Weight, 0);
                setCurrentTarePoint(sensorType_DistanceLength, 0);
                setCurrentTarePoint(sensorType_DistanceWidth, 0);
                setCurrentTarePoint(sensorType_DistanceHeight, 0);
            }

#ifdef _WIN32
            if (1)
            {
                //  Print the cal array
                enableCalibrationCalcPrinting();
                printCalArray(sensorType_DistanceLength);
                printCalArray(sensorType_DistanceWidth);
                printCalArray(sensorType_DistanceHeight);
                printCalArray(sensorType_Weight);
                disableCalibrationCalcPrinting();
            }
#endif // _WIN32

        }

        if (eventTimestamp >= 22095)
        {
            printf("");
        }

        if (eventTimestamp >= 56000)
        {
            printf("");
        }

        if (eventTimestamp >= 55535)
        {
            printf("");
        }

        if (eventTimestamp >= 311903)
        {
            printf("");
        }

        if (returnCode != NULL)
        {
            int32_t     calData;
            int         scanCnt = sscanf(inputLine, ":,%d,%d,%d,%d,%d", &msgType, &eventTimestamp, &data1, &data2, &calData);
            if (3 == scanCnt  ||  4 == scanCnt  ||  5 == scanCnt)
            {
                switch (msgType)
                {
                case 1:
                    if (data2 != 3)
                    {
                        printf("");
                    }
                    sendEvent(msmtMgrEvt_ReportRawSensorMsmt, data1, data2);
                    break;

                case 2:
                    sendEvent(msmtMgrEvt_PeriodicReportTimeout, 0, 0);
                    break;

                case 3:
                    //  Execute the same sequence a serial command would
                    setCurrentTarePoint(sensorType_DistanceWidth,  0);
                    setCurrentTarePoint(sensorType_DistanceHeight, 0);
                    setCurrentTarePoint(sensorType_DistanceLength, 0);
                    setCurrentTarePoint(sensorType_Weight,         0);
                    break;

                case 4:
                    addCalPoint(data1, data2, calData);
                    break;

                case 5:     //  Set the tare point.
                    //  There is no event for this, just function calls
                    setCurrentTarePoint(sensorType_DistanceWidth, 0);
                    setCurrentTarePoint(sensorType_DistanceHeight, 0);
                    setCurrentTarePoint(sensorType_DistanceLength, 0);
                    setCurrentTarePoint(sensorType_Weight, 0);

                    setTareMark(sensorType_DistanceWidth);
                    setTareMark(sensorType_DistanceHeight);
                    setTareMark(sensorType_DistanceLength);
                    setTareMark(sensorType_Weight);
                    break;

                case 6:     //  Report actual raw and filtered values for logging and confirmation that win32 matches arduino
                            //  This reports the testId sensor only
                    setReportedRawAndFilteredValues(data1, data2);
                    break;
                }
                processEvents();
            }
            else
            {
                //  Just ignore the line
//              done = true;
            }
        }
        else
        {
            done = true;
        }
    }
}

#endif // _WIN32
