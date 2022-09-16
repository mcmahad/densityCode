/**
 *
 * ADS1232 library for Arduino
 *
 * MIT License
 *
**/
#include <Arduino.h>
#include "ADS1232.h"

extern HardwareSerial  &dbgSerial;
extern HardwareSerial  &nextionSerial;

// TEENSYDUINO has a port of Dean Camera's ATOMIC_BLOCK macros for AVR to ARM Cortex M3.
#define HAS_ATOMIC_BLOCK (defined(ARDUINO_ARCH_AVR) || defined(TEENSYDUINO))

// Whether we are running on either the ESP8266 or the ESP32.
#define ARCH_ESPRESSIF (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))

// Whether we are actually running on FreeRTOS.
#define IS_FREE_RTOS defined(ARDUINO_ARCH_ESP32)

// Define macro designating whether we're running on a reasonable
// fast CPU and so should slow down sampling from GPIO.
#define FAST_CPU \
    ( \
    ARCH_ESPRESSIF || \
    defined(ARDUINO_ARCH_SAM)     || defined(ARDUINO_ARCH_SAMD) || \
    defined(ARDUINO_ARCH_STM32)   || defined(TEENSYDUINO) \
    )

#if HAS_ATOMIC_BLOCK
// Acquire AVR-specific ATOMIC_BLOCK(ATOMIC_RESTORESTATE) macro.
#include <util/atomic.h>
#endif

#if FAST_CPU
// Make shiftIn() be aware of clockspeed for
// faster CPUs like ESP32, Teensy 3.x and friends.
// See also:
// - https://github.com/bogde/ADS1232/issues/75
// - https://github.com/arduino/Arduino/issues/6561
// - https://community.hiveeyes.org/t/using-bogdans-canonical-hx711-library-on-the-esp32/539
uint8_t shiftInSlow(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder) {
    uint8_t value = 0;
    uint8_t i;

    for(i = 0; i < 8; ++i) {
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(1);
        if(bitOrder == LSBFIRST)
            value |= digitalRead(dataPin) << i;
        else
            value |= digitalRead(dataPin) << (7 - i);
        digitalWrite(clockPin, LOW);
        delayMicroseconds(1);
    }
    return value;
}
#define SHIFTIN_WITH_SPEED_SUPPORT(data,clock,order) shiftInSlow(data,clock,order)
#else
#define SHIFTIN_WITH_SPEED_SUPPORT(data,clock,order) shiftIn(data,clock,order)
#endif


ADS1232::ADS1232() {
}

ADS1232::~ADS1232() {
}

void ADS1232::begin(byte dout, byte pd_sck, byte pdwn, uint8_t mySensorId) {
    PD_SCK   = pd_sck;
    DOUT     = dout;
    PDWN     = pdwn;
    sensorId = mySensorId;
    calibrateOffsetNeeded = false;

    pinMode(DOUT,   INPUT_PULLUP);
    pinMode(PD_SCK, OUTPUT);
    pinMode(PDWN,   OUTPUT);

    power_down();
    power_up();
    power_down();
    //  Initialize the chip as per data sheet sequence if Figure 8.15
    digitalWrite(PDWN,  LOW); delayMicroseconds(20);    //  Minimum is 10 uSec while  low in t15
    digitalWrite(PDWN, HIGH); delayMicroseconds(50);    //  Minimum is 26 uSec while high in t16
    digitalWrite(PDWN,  LOW); delayMicroseconds(50);    //  Minimum is 26 uSec while  low in t17
    digitalWrite(PDWN, HIGH);                           //  Start continuous conversion

    filteredAvg_cnts   = 0;
    filterScaling_Pctg = 100;       //  No filtering, new value only
}

bool ADS1232::is_ready() {
    return digitalRead(DOUT) == LOW;
}

long ADS1232::read() {
    long    returnValue;

    /*  Note : It is asssumed the PD_SCK pin is low before calling shiftIn().  In this application, it is  */
    returnValue  =  ((long)(shiftIn(DOUT, PD_SCK, MSBFIRST))) << 16;
    returnValue |=  ((long)(shiftIn(DOUT, PD_SCK, MSBFIRST))) <<  8;
    returnValue |=  ((long)(shiftIn(DOUT, PD_SCK, MSBFIRST))) <<  0;

    //  We need one more clock to finish data retrieval and start the next conversion
    makeSclkPulse();

    if (calibrateOffsetNeeded)
    {
        /*  Throw in an extra pulse to cause calibration  */
        makeSclkPulse();
        calibrateOffsetNeeded = false;
    }

    if (returnValue == 0xFFFFFF)
    {
        /*  This is a bogus reading, don't process further  */
//      dbgSerial.println(F("ignore bogus reading"));
        return;
    }

    if ((returnValue & 0xFFFF) == 0xFFFF)
    {
        /*  This is a bogus reading, don't process further  */
        dbgSerial.println(F("ignore "));
        return;
    }

    /*  Sign extend from 24-bits to 32-bits  */
    if (returnValue & 0x800000)
    {
        returnValue |= 0xFF000000;
    }

    /*  Feed the new value into an averaging filter.  We have a 24-bit number in a 32-bit value,
        so 8 bits (256) is allowed for scaling.  We will only use a range of [0..100] percent.
    */
    rawMsmt_cnts     = returnValue;
    filteredAvg_cnts = returnValue * filterScaling_Pctg + filteredAvg_cnts * (100 - filterScaling_Pctg);
    filteredAvg_cnts /= 100;

    return returnValue;
}

void ADS1232::makeSclkPulse() {
    digitalWrite(PD_SCK, HIGH);
    digitalWrite(PD_SCK, LOW);
}

void ADS1232::power_down() {
    digitalWrite(PDWN, LOW);
    delayMicroseconds(20);    //  Minimum is 26 uSec while low in t14, figure 8.12
}

bool ADS1232::power_up() {
    bool        returnValue = false;
    if (digitalRead(PDWN) == LOW)
    {
        //  Initialize the chip as per data sheet sequence if Figure 8.15
        digitalWrite(PDWN,  LOW); delayMicroseconds(20);    //  Minimum is 10 uSec while  low in t15
        digitalWrite(PDWN, HIGH); delayMicroseconds(50);    //  Minimum is 26 uSec while high in t16
        digitalWrite(PDWN,  LOW); delayMicroseconds(50);    //  Minimum is 26 uSec while  low in t17
        digitalWrite(PDWN, HIGH);                           //  Start continuous conversion
        returnValue = true;                                 //  We need an autocalibration
    }
    return returnValue;
}

void ADS1232::set_scale(int currentWeight_grams)
{
    scaling_CountsPerGram_x1000 = (1000 * (filteredAvg_cnts - tareValue_cnts))/ currentWeight_grams;

    float   scaling_CountsPerGram_x1000f = (1000.0 * (float)(filteredAvg_cnts - tareValue_cnts))/ (float)currentWeight_grams;
    dbgSerial.println();
    dbgSerial.println();
    dbgSerial.println(F("Scaling : "));
    dbgSerial.print(scaling_CountsPerGram_x1000f);
    dbgSerial.print(F(" counts/gram x 1000 also "));
    dbgSerial.print(scaling_CountsPerGram_x1000);
    dbgSerial.print(F(" count/gram x 1000 "));
    dbgSerial.println();
}
