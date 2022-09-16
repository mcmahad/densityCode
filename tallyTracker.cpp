

#include <stdlib.h>
#include <stdbool.h>

#include "timerEvtHandler.h"
#include "tallyTracker.h"
#include "calibration.h"

#ifndef _WIN32
#include <arduino.h>
#else
#include "win32shims.h"
#endif

#ifdef  _WIN32
static  HardwareSerial      dbgSerial,
                            nextionSerial;
#else
extern  HardwareSerial  &dbgSerial,
                        &nextionSerial;
#endif  //  _WIN32


#define     NUM_STICKS_UNTIL_FLUSH      150

static uint32_t getValidationKey(int32_t newKey);

static  int32_t     sticksRemaining,
                    countsRemainingSticksUntilEepromWrite,
                    inventoryKey;


static char *ultoa(unsigned long val, char *s, unsigned radix)
{
    int c;

    if (val >= radix)
    {
        s = ultoa (val / radix, s, radix);
    }
    c = val % radix;
    c += (c < 10 ? '0' : 'a' - 10);
    *s++ = c;
    return s;
}


void tallyTrkObj_Initialize(void)
{
    if (sticksRemaining <= 0)
    {
        sticksRemaining = 0;
    }
}


static bool     measureTallyState = false;

/*    1)  User selects tally screen
 *    2)  Tally screen sends tallyStatus,1 to request a new inventory key
 *    3)  A new inventory key is created by grabbing the current mSec counter and masking lower 16 bits
 *    4)  New inventory key is sent to show on Nextion screen (confirmCode.val=xxx)
 *    5)  User reads the key and gives it to sinoPro
 *    6)  SinoPro creates a validation response, large decimal value
 *    7)  SinoPro sends validation response back to the user
 *    8)  User enters validation response as a number into Nextion
 *    9)  Nextion sends validation response to Arduino (what message name? tallyValid,xxxxx  ?
 *   10)    Arduino confirms the validation response.  If good, extract enum for new stick count
 *   11)    Arduino converts stick count enum to a number of sticks and applies that value
 *
 *   12)     final,0 is sent when leaving  the main screen
 *   13)     final,1 is sent when entering the main screen, should force to tally screen if count is zero
 *
 *
 *
 *               Encryption layout
 *
 *    Collect timestamp for inventory key, send it to host modulo 120,000.  After receiving it, modulo it
 *    with 65535 before using it.
 *
 *    15 bits - inventory key to match
 *     4 bits - enum for bundle size
 *    12 bits - 0x28F
 *  ---------
 *    31 bits
 *
 *  31 . . . . . . . . . . . . . . . . 0
 *  |***************|****|************|
 *          |         |            |
 *         |         |            +------- 12 bits : 0x28F
 *        |         +---------------------  4 bits : bundle size enum
 *       +-------------------------------- 15 bits : Inventory key (16 bits)
 *
 *
 *           Bundle sizes
 *      =========================
 *       10   200   5000   100000
 *       20   500  10000   200000
 *       50  1000  20000   500000
 *      100  2000  50000  1000000
 *
 *     To confirm the key, check 0x28F and inventory key for correctness, if they match then use bundle size
 *
 */

void tallyTrkObj_EventHandler(eventQueue_t* event)
{
    switch (event->eventId)
    {
    case tallyTrkEvt_SetCountRemaining:
        //  data1 : the remaining count to use
#ifdef SET_TALLY0_TO_TALLY10
        if (event->data1 == 0)
        {   // For testing
            event->data1 = 10;
        }
#endif //  SET_TALLY0_TO_TALLY10

        sticksRemaining = event->data1;
        if (sticksRemaining <= 0) sticksRemaining = 0;
        countsRemainingSticksUntilEepromWrite = NUM_STICKS_UNTIL_FLUSH;
        dbgSerial.print(F("Set stick count to "));
        dbgSerial.println(sticksRemaining);
        break;

    case tallyTrkEvt_DensityPublished:
        //  data1 : the density x10
        //  This event is sent whenever a density measurement is published
        if (!measureTallyState)
        {
            sendEvent(tallyTrkEvt_CountOneStick, 0, 0);     //  Count this as a stick
            measureTallyState = true;
            sendEvent(binningEvt_ShowDensityBall, event->data1, 0);
            dbgSerial.println(F("Density published"));
        }
        break;

    case tallyTrkEvt_DensityCleared:
        //  This event is sent when all sensors report tare at the same time.
        //  Reset the trigger
        measureTallyState = false;
        sendEvent(binningEvt_ClearDensityBall, 0, 0);
        break;

    case tallyTrkEvt_CountOneStick:
        dbgSerial.println(F("Decrement stick Count"));
        if (sticksRemaining > 0) sticksRemaining--;
        if (countsRemainingSticksUntilEepromWrite > 0) countsRemainingSticksUntilEepromWrite--;

        if (countsRemainingSticksUntilEepromWrite <= 0  || sticksRemaining == 0)
        {
            //  Write sticksRemaining to EEPROM
            sendEvent(tallyTrkEvt_SaveTallyToEeprom, 0, 0);
            countsRemainingSticksUntilEepromWrite = NUM_STICKS_UNTIL_FLUSH;
        }
        if (sticksRemaining == 0)
        {
            //  No more sticks remaining, send us to the license screen
            sendEvent(msmtMgrEvt_MoveToLicenseScreen, 0, 0);
        }
        sendEvent(timerEvt_cancelTimer, tallyTrkEvt_InactivityTimeout, 0);
        sendEvent(timerEvt_startTimer, tallyTrkEvt_InactivityTimeout, 1000 * 60 * 10);
        break;

    case tallyTrkEvt_InactivityTimeout:
        sendEvent(tallyTrkEvt_SaveTallyToEeprom, 0, 0);
        break;

    case tallyTrkEvt_SaveTallyToEeprom:
        writeCalDataToFlash();
        sendEvent(timerEvt_cancelTimer, tallyTrkEvt_InactivityTimeout, 0);
        sendEvent(timerEvt_startTimer, tallyTrkEvt_InactivityTimeout, 1000 * 60 * 10);
        break;

    case tallyTrkEvt_TallyStatusEntry:
        sendEvent(tallyTrkEvt_CreateNewInventoryKey, 0, 0);
        sendEvent(tallyTrkEvt_UpdateTallyStatusScreen, 0, 0);
        break;

    case tallyTrkEvt_TallyStatusExit:
        break;

    case tallyTrkEvt_UpdateTallyStatusScreen:
        nextionSerial.print(F("stixRemain.val="));
        nextionSerial.print(tallyTrkObj_GetStickCount());
        nextionSerial.print(F("\xFF\xFF\xFF"));

        nextionSerial.print(F("confirmCode.val="));
#ifdef _WIN32
        nextionSerial.print(inventoryKey % 233000);
#else
        nextionSerial.print((long)(inventoryKey % 233000));
#endif // WIN32

        nextionSerial.print(F("\xFF\xFF\xFF"));
        break;

    case tallyTrkEvt_CreateNewInventoryKey:
        /*   Create a new random key by using the current timestamp
        */
        inventoryKey = getTime_mSec() % 233000;
        dbgSerial.print(F("New inventoryKey is "));
#ifdef _WIN32
        dbgSerial.print(inventoryKey);
#else
        dbgSerial.print((long)inventoryKey);

        dbgSerial.print(F("    validation key="));

//      dbgSerial.print((long)getValidationKey(inventoryKey));
        {
            char    intString[16];
            ultoa(getValidationKey(inventoryKey), intString, 10);
            dbgSerial.print(intString);
        }

#endif // _WIN32

        dbgSerial.println();

        sendEvent(tallyTrkEvt_UpdateTallyStatusScreen, 0, 0);
        break;

    case tallyTrkEvt_TallyValid:
        /*  data1 : Contains the user entered validation key
         *
         *  This key needs to be confirmed as valid and extract the new stick count
         *  enumeration to specify the number of sticks to adopt.  We use the
         *  retained inventoryKey to confirm against, as described above.
         */
         {
            uint32_t    keyToMatch = ((inventoryKey & 0x7FFF) << 16) |
                                      (               0x028F  <<  0);

            dbgSerial.print(F("TallyValid : "));
            dbgSerial.print(F("inventoryKey="));
            dbgSerial.print(inventoryKey);
            dbgSerial.print(F("   KeyToMatch="));
            dbgSerial.print(keyToMatch);
            dbgSerial.print(F("\n"));

            event->data1 ^= 0x28F28F;   //  Throw in some random bit flipping to confuse things
            if (keyToMatch == (event->data1 & 0x7FFF0FFF))
            {
                //  The key matches, convert the bundle size enum to the new stick count
                static  const   int32_t     possibleStickCount[] =
                {
                            10,     20,     50,
                           100,    200,    500,
                          1000,   2000,   5000,
                         10000,  20000,  50000,
                        100000, 200000, 500000,
                       1000000
                };

                int     newStickCountIndex = (event->data1 >> 12) & 0x0F;
                int32_t newStickCount;

                newStickCountIndex = max(0, min( 15, newStickCountIndex));
                newStickCount      = possibleStickCount[newStickCountIndex];

                //  Set a new stick count
                sendEvent(tallyTrkEvt_SetCountRemaining,       newStickCount, 0);
                sendEvent(tallyTrkEvt_SaveTallyToEeprom,       0,             0);
                sendEvent(tallyTrkEvt_UpdateTallyStatusScreen, 0,             0);
                dbgSerial.print(F("Stick count updated to use Index="));
                dbgSerial.print(newStickCountIndex);
                dbgSerial.print(F("  Cnt="));
#ifdef _WIN32
                dbgSerial.print(newStickCount);
#else
                dbgSerial.print((long)newStickCount);
#endif // _WIN32

                dbgSerial.println(F("\n"));

                nextionSerial.print(F("valBox.txt=\"Good\""));
                nextionSerial.print(F("\xFF\xFF\xFF"));
                nextionSerial.print(F("valBox.pco=1600"));
                nextionSerial.print(F("\xFF\xFF\xFF"));
            }
            else
            {
                dbgSerial.println(F("Stick count attempted, did not match\n"));

                nextionSerial.print(F("valBox.txt=\"Try Again\""));
                nextionSerial.print(F("\xFF\xFF\xFF"));
                nextionSerial.print(F("valBox.pco=63488"));
                nextionSerial.print(F("\xFF\xFF\xFF"));
            }
         }
        break;

    default:
        break;
    }
}


int32_t tallyTrkObj_GetStickCount(void)
{
    return sticksRemaining;
}


static uint32_t getValidationKey(int32_t newKey)
{

    uint32_t    returnValue;

    returnValue  = ((newKey & 0x7FFF) << 16);
    returnValue +=  (         0x028F  <<  0);
    returnValue +=  (              3  << 12);
    returnValue ^=  (              0x28F28F);

/*
    dbgSerial.print(F("getValKey: newKey="));
    dbgSerial.print(newKey);
    dbgSerial.print(F("  "));
    dbgSerial.print(returnValue);
*/
    return returnValue;
}


