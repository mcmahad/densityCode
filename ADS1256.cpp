

//       Default is 60 samples/sec if no choice is made
//  #define CHINA_MODE
    #define ADS1256_60SPS_SAMPLING
//  #define ADS1256_50SPS_SAMPLING
//  #define ADS1256_30SPS_SAMPLING
//  #define ADS1256_25SPS_SAMPLING
//  #define ADS1256_15SPS_SAMPLING
//  #define ADS1256_10SPS_SAMPLING
//  #define ADS1256_5SPS_SAMPLING
//  #define ADS1256_2p5SPS_SAMPLING


#include "commonHeader.h"
#include <Arduino.h>
#include <stdint.h>
#include "ads1256.h"

extern HardwareSerial  &dbgSerial;
extern HardwareSerial  &nextionSerial;


typedef enum
{
    spiCmd_Wakeup                   = 0x00,
    spiCmd_ReadData                 = 0x01,
    spiCmd_ReadDataContinuously     = 0x03,
    spiCmd_StopDataContinuously     = 0x0F,
    spiCmd_ReadReg                  = 0x10,
    spiCmd_WriteReg                 = 0x50,
    spiCmd_SelfCalibration          = 0xF0,
    spiCmd_OffsetSelfCalibration    = 0xF1,
    spiCmd_GainSelfCalibration      = 0xF2,
    spiCmd_SystemOffsetCalibration  = 0xF3,
    spiCmd_SystemGainCalibration    = 0xF4,
    spiCmd_SynchronizeCalibration   = 0xFC,
    spiCmd_BeginStandbyMode         = 0xFD,
    spiCmd_ResetToPowerup           = 0xFE,
    spiCmd_Wakeup2                  = 0xFF,
} spiCmd_t;

#define     MUX_CTRL_REG         1
#define NOP __asm__ __volatile__ ("nop\n\t")

static  const   uint8_t startingCmdSequence[] =
{
    0x00,           /*  Reg 0, status 0x01 : DRDY*, status only, not writeable
                                        02 : BUFEN, set to enable buffer,
                                        04 : ACAL, enable self-calibration for changes to PGA, DataRate, or BufEn
                                        08 : ORDER, Data bit order, 0=MSB first, 1=LSB first
                    */
                    //                                                                 2 is AIN2
    0x28,           //  Reg 1, mux register  :  High nibble is positive input channel, 1 is AIN1
                    //                                                                 0 is AIN0
                    //                       :  Low nibble is negative input channel, 8 is AINCOM common channel
                    //
    0x00,           /*  Reg 2, A/D control register : 2 is Fclkin/1
                                                      programmable gain selection is 001, gain is 2
                                                      programmable gain selection is 000, gain is 1
                    */
#if     defined(ADS1256_60SPS_SAMPLING)
    0x72,           //  Reg 3, A/D data rate (60 samples/sec)
#elif   defined(ADS1256_50SPS_SAMPLING)  ||  defined(CHINA_MODE)
    0x63,
#elif   defined(ADS1256_30SPS_SAMPLING)
    0x53,
#elif   defined(ADS1256_25SPS_SAMPLING)
    0x43,
#elif   defined(ADS1256_15SPS_SAMPLING)
    0x33,
#elif   defined(ADS1256_10SPS_SAMPLING)
    0x23,
#elif   defined(ADS1256_5SPS_SAMPLING)
    0x13,
#elif   defined(ADS1256_2p5SPS_SAMPLING)
    0x03,
#else
    0x72,           //  Reg 3, A/D data rate (60 samples/sec)
#endif
    0x00,           //  Reg 4, GPIO control register
    //  Don't set any other registers for offset calibration
};

static  const   uint8_t enablePgaSequence[] =
{
    0x00,           /*  Reg 0, status 0x01 : DRDY*, status only, not writeable
                                        02 : BUFEN, set to enable buffer,
                                        04 : ACAL, enable self-calibration for changes to PGA, DataRate, or BufEn
                                        08 : ORDER, Data bit order, 0=MSB first, 1=LSB first
                    */
};

static  const   uint8_t selectBestGain[] =
{
    0x00,
    0x00,
    0x48        //  Most signification byte (register 10) must be 72 decimal, 0x48

    //   0xE8A38C - 2.5 volts

/************  With backward values  *****************************/
    //   0x200000 - Best we get is 0x2B
    //   0x800000 - Best we got is AA
    //   0x8C0000 - Best we got is BA
    //   0x8CA3E8 - 1.5 volts
    //   0xA91945 - Best we get is 0x5C0000
    //   0xFFFFFF - 1.4 volts
};

void ads1256::SPI_transfer(uint8_t value)
{
    int     mosiBitmask   = digitalPinToBitMask( mosi_pin   ),
            spiclkBitmask = digitalPinToBitMask( spiclk_pin );

    volatile uint8_t *spiclkOutPtr = portOutputRegister(digitalPinToPort(spiclk_pin));
    volatile uint8_t *mosiOutPtr   = portOutputRegister(digitalPinToPort(mosi_pin  ));

    uint8_t oldSREG = SREG;


    cli();
    for (int index = 8; index !=0; index--)
    {
        //  Set or clear the bit
        if (value &0x80)
        {
            *mosiOutPtr |=  mosiBitmask;
        }
        else
        {
            *mosiOutPtr &= ~mosiBitmask;
        }

        //  Toggle SPI clock high and low
        *spiclkOutPtr |= spiclkBitmask;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        *spiclkOutPtr &= ~spiclkBitmask;

        value <<= 1;
    }

    SREG = oldSREG;
    sei();
}


uint8_t ads1256::SPI_transfer_read(void)
{
    int     misoBitmask   = digitalPinToBitMask( miso_pin   ),
            spiclkBitmask = digitalPinToBitMask( spiclk_pin );

    volatile uint8_t *spiclkOutPtr = portOutputRegister(digitalPinToPort(spiclk_pin));

    volatile uint8_t *inputRegPtr = portInputRegister(digitalPinToPort(miso_pin));

    uint8_t returnValue = 0,
            mask        = 0x80;

    uint8_t oldSREG = SREG;

    cli();

    for (int index = 0; index < 8; index++)
    {
        //  Toggle SPI clock high and low
        *spiclkOutPtr |= spiclkBitmask;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        *spiclkOutPtr &= ~spiclkBitmask;

        if (*inputRegPtr & misoBitmask)
        {
            returnValue |= mask;
        }
        mask >>= 1;
    }

    SREG = oldSREG;
    sei();

    return returnValue;
}


void ads1256::ads1256_Init(uint8_t spiChipSelPin, uint8_t dataReadyPin, uint8_t syncPdwnPin, uint8_t resetPin)
{
    /*  For Arduino Uno, pins are fixed by the hardware of the CPU design.  Pin allocations are:

                ChipSelect  - 10
                MOSI        - 11
                MISO        - 12
                SCK         - 13
    */
    int32_t     startTime;

    spiChipSel_pin     = spiChipSelPin;
    dataReady_pin      = dataReadyPin;
    syncDown_pin       = syncPdwnPin;
    reset_pin          = resetPin;
    currentChannel     = 0;
    drdyTimeDelay_mSec = 16;

    //  Configure GPIO pins and set outputs to starting values
    if (spiChipSel_pin >= 0) pinMode(spiChipSel_pin, OUTPUT);
    if (dataReady_pin  >= 0) pinMode(dataReady_pin,  INPUT );
    if (syncDown_pin   >= 0) pinMode(syncDown_pin,   OUTPUT);
    if (reset_pin      >= 0) pinMode(reset_pin,      OUTPUT);

    digitalWrite(spiChipSelPin, HIGH);
    digitalWrite(syncDown_pin,  HIGH);
    digitalWrite(reset_pin,     HIGH);

    //  Hard code the pins that were working on  Uno for  now, maybe change them later
    mosi_pin   = 11;    //#define SPI_MOSI_PIN     11   //  Use DIO11 on the Arduino Uno
    miso_pin   = 12;    //#define SPI_MISO_PIN     12   //  Use DIO12 on the Arduino Uno
    spiclk_pin = 13;    //#define SPI_CLOCK_PIN    13   //  Use DIO13 on the Arduino Uno

    pinMode(spiclk_pin, OUTPUT);
    pinMode(mosi_pin,   OUTPUT);
    pinMode(miso_pin,   INPUT);

    delayMicroseconds(20);

    {
        int limitCount = 0;
        do
        {
            delayMicroseconds(2000);   //  Start with some dead time

            //  Start with a hardware reset
            digitalWrite(reset_pin, LOW );
            delayMicroseconds(1);   //  Should be low 4 clocks @ 50 MHz, half a microsecond
            digitalWrite(reset_pin, HIGH);

            /*  Wait for self-calibration  to  finish and conversion starts on it's own  */
            startTime = millis();
            while (!is_DataReady()  &&  ((millis() - startTime) < 1000L));

            ads1256_Reg_Write(0, startingCmdSequence, sizeof(startingCmdSequence)/sizeof(startingCmdSequence[0]), true);
            //  ads1256_Reg_Write(0, startingCmdSequence, 5, true);

            //  Wait for data to be ready before the next step
            startTime = millis();
            while (!is_DataReady()  &&  ((millis() - startTime) < 700));

            limitCount++;
        }
        while (!is_DataReady()  &&  limitCount < 3);
    }

    {
        //  Read back all the registers, just to see what they are
        uint8_t     readData[12];

        ads1256_Reg_Read(0, readData, 11);
    }

    //  Perform internal offset calibration
    digitalWrite(spiChipSelPin, LOW );
    SPI_transfer(spiCmd_OffsetSelfCalibration);
    digitalWrite(spiChipSelPin, HIGH);

    //  Wait for calibration to complete
    startTime = millis();
    while (!is_DataReady()  &&  (millis() - startTime) < 2000);

#ifdef  DONT_DO
    //  Perform internal gain calibration
    digitalWrite(spiChipSelPin, LOW );
    SPI_transfer(spiCmd_GainSelfCalibration);
    digitalWrite(spiChipSelPin, HIGH);

    //  Wait for calibration to complete
    startTime = millis();
    while (!is_DataReady()  &&  (millis() - startTime) < 2000);
#endif

    //  Turn the buffer on, now that calibration is done
    //  Do NOT turn on the buffer if the max input voltage
    //  is >3.0 volts.   With buffer off, it can go up to 6.0 volts.
//  ads1256_Reg_Write(0, enablePgaSequence, 1, true);

    //  Wait some time, just because
    delayMicroseconds(50);

//  ads1256_SetNewChannelGain(0x100000);
//  ads1256_SetNewChannelGain(0x4651F0);    //  Picked for max range on ADC
//  ads1256_SetNewChannelGain(0x8CA3E0);    //  Picked for max range on ADC
    ads1256_SetNewChannelGain(0x4651F00);    //  Picked for 8x max range on ADC

    //  No setting th new offset for now
//  ads1256_SetNewChannelOffset(0);
//  ads1256_SetNewChannelOffset(0xF0F008);
//  ads1256_SetNewChannelOffset(0xFFFFF0);
    ads1256_SetNewChannelOffset(0);


    /*  Perform a system offset calibration.  AIN5 and AIN6 are connected
        together and also to ground.  Select them to perform system offset calibration
    */
    uint8_t     rawBytes[4];            //  One byte for data, +3 data for safety

    //  Construct the new mux register and send it to the ADC
    rawBytes[0] = (0x6 << 4) | 0x05;
    ads1256_Reg_Write(MUX_CTRL_REG, rawBytes, 1, false);

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

    //  ChipSelect low to start the SPI transaction
    digitalWrite(spiChipSel_pin, LOW);

    //  Send the OffsetSelfCalibration command
    SPI_transfer(spiCmd_OffsetSelfCalibration);

    //  ChipSelect high to start the offset calibration
    digitalWrite(spiChipSel_pin, HIGH);

    //  Wait some time, just because
    delayMicroseconds(50);

    //  Wait for DRDY before continuing.  This can take up to 800.3 mSec
    //  for 2.5 SPS
    startTime = millis();
    while (!is_DataReady()  &&  (millis() - startTime) < 1000);


    /*  Perform a system gain calibration.  AIN6 is connected to ground, AIN7 is connected
        to +5V from low noise regulagtor.
    */
    //  Construct the new mux register and send it to the ADC
    rawBytes[0] = (0x07 << 4) | 0x06;
    ads1256_Reg_Write(MUX_CTRL_REG, rawBytes, 1, false);

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

    //  ChipSelect low to start the SPI transaction
    digitalWrite(spiChipSel_pin, LOW);

    //  Send the GainSelfCalibration command
    SPI_transfer(spiCmd_GainSelfCalibration);

    //  ChipSelect high to start the gain calibration
    digitalWrite(spiChipSel_pin, HIGH);

    //  Wait some time, just because
    delayMicroseconds(50);

    //  Wait for DRDY before continuing.  This can take up to 800.4 mSec
    //  for 2.5 SPS
    startTime = millis();
    while (!is_DataReady()  &&  (millis() - startTime) < 1000);
    {
        //  Read back all the registers, just to see what they are
        uint8_t     readData[12];

        ads1256_Reg_Read(0, readData, 11);
    }

    //  Wait some time, just because
    delayMicroseconds(50);

    digitalWrite(syncDown_pin, LOW );    //  Set SYNC low to reset digital filter
    delayMicroseconds(2);                //  We need to wait for 4 clocks at 8 MHz, or 0.5 uSec
    digitalWrite(syncDown_pin, HIGH);    //  Set SYNC high to start the conversion.

    //  DRDY should go high soon as the start of the first conversion
}


void ads1256::ads1256_Reg_Write(uint8_t readWriteAddress, uint8_t *srcDataPtr,  uint8_t dataLength, bool raiseChipSelAtEnd)
{
    //  ChipSelect low to start the SPI transaction
    digitalWrite(spiChipSel_pin, LOW );

    //  Select the starting AD1256 register to write
    SPI_transfer(spiCmd_WriteReg + readWriteAddress);

//   Writes don't need a delay here
//  //  Need to wait for at least 50 clock periods @ 8 Mhz = 6.25 uSec before continuing
//  delayMicroseconds(10);

    //  Write number of bytes to expect, minus one
    SPI_transfer(dataLength - 1);

    //  Transfer all bytes
    for (int index = 0; index < dataLength; index++)
    {
        SPI_transfer(*srcDataPtr++);    //  No return value, this is write only
    }

    if (raiseChipSelAtEnd)
    {
        //  ChipSelect high to end the SPI transaction
        digitalWrite(spiChipSel_pin, HIGH);
    }
}


void ads1256::ads1256_Reg_Read(uint8_t readWriteAddress, uint8_t *destDataPtr, uint8_t dataLength)
{
    //  ChipSelect low to start the SPI transaction
    digitalWrite(spiChipSel_pin, LOW);

    //  Select the starting AD1256 register to read
    SPI_transfer(spiCmd_ReadReg + readWriteAddress);

    //  Write number of bytes to expect, minus one
    SPI_transfer(dataLength - 1);

    //  Need to wait for at least 50 clock periods @ 8 Mhz = 6.25 uSec before continuing
    delayMicroseconds(17);

    //  Transfer all bytes
    for (int index = 0; index < dataLength; index++)
    {
        *destDataPtr++ = SPI_transfer_read();
    }

    //  ChipSelect high to end the SPI transaction
    digitalWrite(spiChipSel_pin, HIGH);
}


int ads1256::getFirstDrdyDelayTime(void)
{
    return drdyTimeDelay_mSec;
}


void ads1256::addToFirstDrdyDelayTime(int adjValue)
{
    int newValue = drdyTimeDelay_mSec + adjValue;

    if (newValue >= 14  &&  newValue <= 18)
    {
        drdyTimeDelay_mSec = newValue;
    }
}


int ads1256::getCurrentChannelNumber(void)
{
    return currentChannel;
}


bool ads1256::is_DataReady(void)
{
    return (digitalRead(dataReady_pin) == LOW) ? true : false;
}


int32_t ads1256::readConvertedDataAndStartNextChannel(void)
{
    int32_t     returnValue = 0;
    uint8_t     rawBytes[4];            //  One byte for data, +3 data for safety

    /*  The recommended order in the data sheet is:
        o  Setting the next channel
        o  Triggering start of the next measurement with SYNC
        o  read and return the previous channel
    */

    currentChannel++;
    if (currentChannel >= ACTIVE_ADC_CHANNEL_CNT)
    {
        currentChannel   = 0;
    }

//  #define  USE_SINGLE_CHANNEL2
#ifdef  USE_SINGLE_CHANNEL2

    currentChannel = 2;     //  Range is [0..2]

    {
        static  uint8_t   executeCnt;

        if (executeCnt < 1)
        {
            executeCnt++;

            //  Construct the new mux register and send it to the ADC
            rawBytes[0] = (0x00 << 4) | 0x08;  //  AIN0 and Common, AIN0 connected to Vref (2.50)
//          rawBytes[0] = (0x01 << 4) | 0x08;  //  AIN1 and Common, AIN1 connected to Common
//          rawBytes[0] = (0x02 << 4) | 0x08;  //  AIN2 and Common, AIN2 connected to pot wiper
//          rawBytes[0] = (0x03 << 4) | 0x08;  //  AIN3 and Common, AIN3 connected to Common
//          rawBytes[0] = (0x04 << 4) | 0x08;  //  AIN4 and Common, AIN4 connected to Common
//          rawBytes[0] = (0x05 << 4) | 0x08;  //  AIN5 and Common, AIN5 connected to Common
//          rawBytes[0] = (0x06 << 4) | 0x08;  //  AIN6 and Common, AIN6 connected to Common
//          rawBytes[0] = (0x07 << 4) | 0x08;  //  AIN7 and Common, AIN7 connected to AVDD (5.0v)

            ads1256_Reg_Write(MUX_CTRL_REG, rawBytes, 1, false);

            //  Shouldn't be needed, but add some delay to be safe
            delayMicroseconds(10);
        }
        else
        {
            //  Need to assert ChipSelect low here, it needs to be done to start the SPI transaction
            digitalWrite(spiChipSel_pin, LOW );
        }
    }
#else
    //  Construct the new mux register and send it to the ADC
    rawBytes[0] = (currentChannel << 4) | 0x08;
    ads1256_Reg_Write(MUX_CTRL_REG, rawBytes, 1, false);

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

#endif

    //  Triggering start of the next measurement with SYNC
    SPI_transfer(spiCmd_SynchronizeCalibration);
    /*  Need to wait 24 clocks, or 3.125 uSec, for t11.  Software slowness
        already guarantees that delay but this line is included to explicitly
        recognize that
    */
    delayMicroseconds(10);
    SPI_transfer(spiCmd_Wakeup);

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

    //  Read the ADC contents with the RDATA command
    returnValue = ads1256_ReadData();

    return returnValue;
}


int32_t ads1256::ads1256_ReadData(void)
{
    int32_t     returnValue = 0;
    uint8_t     newVal;

    //  ChipSelect low to start the SPI transaction
    digitalWrite(spiChipSel_pin, LOW);

    //  Send the RDATA command
    SPI_transfer(spiCmd_ReadData);

    //  Need to wait for at least 50 clock periods @ 8 Mhz = 6.25 uSec before continuing
    delayMicroseconds(10);

    //  read three bytes (twenty-four bits) while sending 0 as an output value
    returnValue = SPI_transfer_read();
    returnValue = (returnValue << 8) + SPI_transfer_read();
    returnValue = (returnValue << 8) + SPI_transfer_read();

    //  Sign extend from 24 bits to 32 bits
    if (returnValue & 0x800000) returnValue |= 0xFF000000;

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

    //  ChipSelect high to end the SPI transaction
    digitalWrite(spiChipSel_pin, HIGH);

    //  There should be a delay for t11 here, 4 clocks @ 50 MHz.  Not included, since
    //  it is too short to worry about.

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);

    return returnValue;
}


void ads1256::ads1256_RestartCurrentMeasurement(void)
{
    /*  There was something going on that may have caused noise or other reason,
     *   so restart the current ADC conversion cycle with the same channel.
     *
     *   This is done by sending the sync command followed by the wakeup.
     */

    //  ChipSelect low to end the SPI transaction
    digitalWrite(spiChipSel_pin, LOW);

    //  Restart the current measurement with SYNC
    SPI_transfer(spiCmd_SynchronizeCalibration);
    /*  Need to wait 24 clocks, or 3.125 uSec, for t11.  Software slowness
        already guarantees that delay but this line is included to explicitly
        recognize that
    */
    delayMicroseconds(10);
    SPI_transfer(spiCmd_Wakeup);

    //  ChipSelect high to end the SPI transaction
    digitalWrite(spiChipSel_pin, HIGH);

    //  Shouldn't be needed, but add some delay to be safe
    delayMicroseconds(10);}

void ads1256::ads1256_SetNewChannelGain(int32_t newOffset)
{
    /*  New gain is a 24-bit unsigned value.  The gain registers
        are byte reversed and start at address 8.
    */

    uint8_t gainBuffer[3 + 1];      //  3 minimum, + 1 for safety

//  dbgSerial.print("NewGain=");
//  dbgSerial.println(newOffset,HEX);

    gainBuffer[0] = newOffset & 0xFF;
    newOffset     >>= 8;
    gainBuffer[1] = newOffset & 0xFF;
    newOffset     >>= 8;
    gainBuffer[2] = newOffset & 0xFF;

    ads1256_Reg_Write(8, gainBuffer, 3, true);

    //  Wait some time, just because
    delayMicroseconds(50);

}


void ads1256::ads1256_SetNewChannelOffset(uint32_t newOffset)
{
    /*  New gain is a 24-bit unsigned value, +/- 8.4Million.  The
        offset registers are byte reversed and start at address 5.
    */

    uint8_t offsetBuffer[3 + 1];      //  3 minimum, + 1 for safety

//  dbgSerial.print("NewOffset=");
//  dbgSerial.println(newOffset,HEX);

    offsetBuffer[0] = newOffset & 0xFF;
    newOffset     >>= 8;
    offsetBuffer[1] = newOffset & 0xFF;
    newOffset     >>= 8;
    offsetBuffer[2] = newOffset & 0xFF;

    ads1256_Reg_Write(5, offsetBuffer, 3, true);

    //  Wait some time, just because
    delayMicroseconds(50);

}
