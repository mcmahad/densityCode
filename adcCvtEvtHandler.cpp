

#include "events.h"
#include "adcCvtEvtHandler.h"
#include "ADS1256.h"
#include "calibration.h"

//#define SPI_MOSI_PIN     11   //  Always DIO11 on the Arduino Uno
//#define SPI_MISO_PIN     12   //  Always DIO12 on the Arduino Uno
//#define SPI_CLOCK_PIN    13   //  Always DIO13 on the Arduino Uno

#define SPI_CHIPSEL_PIN   10  //  Any available output pin is allowed
#define DATAREADY_PIN     7   //  Any available output pin is allowed
#define SYNCDOWN_PIN      9   //  Any available output pin is allowed
#define RESET_PIN         8   //  Any available output pin is allowed

static  ads1256   adcConverter;

static  int32_t   rawAdcReading[ACTIVE_ADC_CHANNEL_CNT];

extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;

void adcCvtObj_Initialize(void)
{
    adcConverter.ads1256_Init(SPI_CHIPSEL_PIN, DATAREADY_PIN, SYNCDOWN_PIN, RESET_PIN);

    sendEvent(timerEvt_startTimer,  adcCvtEvt_WaitConverterReadyTimeout, 12);

    sendEvent(timerEvt_startTimer,  adcCvtEvt_WatchdogTimeout, 1000);
//  sendEvent(timerEvt_startPeriodicTimer, adcCvtEvt_BumpGain, 1000);
}


void adcCvtObj_EventHandler(eventQueue_t* event)
{
    uint8_t   readBuff[16];

    if (0  &&  event->eventId != adcCvtEvt_WaitConverterReadyTimeout  &&  event->eventId != adcCvtEvt_RestartCurrentConversionTimeout)
    {
        dbgSerial.print(F("ADC "));
        dbgSerial.println(event->eventId, HEX);
    }

    switch (event->eventId)
    {
    case adcCvtEvt_InitializeConverter:
        adcConverter.ads1256_Init(SPI_CHIPSEL_PIN, DATAREADY_PIN, SYNCDOWN_PIN, RESET_PIN);
        cancelTimer(adcCvtEvt_WatchdogTimeout);
        cancelTimer(adcCvtEvt_RestartCurrentConversionTimeout);
        cancelTimer(adcCvtEvt_WaitConverterReadyTimeout);
        sendEvent(timerEvt_startTimer, adcCvtEvt_WaitConverterReadyTimeout, 12);
        sendEvent(timerEvt_startTimer, adcCvtEvt_WatchdogTimeout, 1000);
        break;

    case adcCvtEvt_BumpGain:
        {
            static  uint8_t     nextCount = 0;

            adcConverter.ads1256_SetNewChannelGain((long)nextCount << 16L);
//          dbgSerial.print("GainCnt = ");
//          dbgSerial.println(nextCount);
            nextCount++;
        }
        break;

    case adcCvtEvt_WatchdogTimeout:
        sendEvent(adcCvtEvt_InitializeConverter, 0, 0);
        break;

    case adcCvtEvt_WaitConverterReadyTimeout:

//      digitalWrite(10, HIGH);
//      digitalWrite(10, LOW );

        //  Make sure outgoing serial buffer is empty
        if (1)
        {
            static  uint16_t  maxSerialBuffSize     = 10; //  Start small, it will grow
                    uint16_t  currentSerialBuffSize = nextionSerial.availableForWrite();

            if (currentSerialBuffSize < maxSerialBuffSize)
            {
                //  Still some serial data to send, wait another millisecond
                sendEvent(timerEvt_cancelTimer, adcCvtEvt_WaitConverterReadyTimeout, 0);
                sendEvent(timerEvt_startTimer,  adcCvtEvt_WaitConverterReadyTimeout, 1);
//              dbgSerial.print(F("X"));
            }

            if (maxSerialBuffSize < currentSerialBuffSize)
            {   //  Grow the max size, if needed
                maxSerialBuffSize = currentSerialBuffSize;
            }
        }

        if (adcConverter.is_DataReady())
        {
//          digitalWrite(10, HIGH);
//          digitalWrite(10, LOW );
//          digitalWrite(10, HIGH);
//          digitalWrite(10, LOW );

//          adcConverter.addToFirstDrdyDelayTime(-1);

            /*  Hardware signals say data is ready to read, process it and trigger the next channel  */
            int       currentChannel = adcConverter.getCurrentChannelNumber();    //  Remember the channel we are about to read from
            int32_t         oldValue = rawAdcReading[currentChannel];

            rawAdcReading[currentChannel] = adcConverter.readConvertedDataAndStartNextChannel();
            sendEvent(timerEvt_startTimer, adcCvtEvt_WaitConverterReadyTimeout, 12);

//          sendEvent(timerEvt_cancelTimer, adcCvtEvt_RestartCurrentConversionTimeout, 0);
//          sendEvent(timerEvt_startTimer,  adcCvtEvt_RestartCurrentConversionTimeout, 5);

            //  DMMDBG- print the raw measurements
            if (0  &&  currentChannel == 2)
            {
                dbgSerial.print(F(","));
                dbgSerial.println(rawAdcReading[currentChannel]);
            }

//          if (adcConverter.getFirstDrdyDelayTime() != 16)
//          {
//              dbgSerial.print("1stDly=");
//              dbgSerial.println(adcConverter.getFirstDrdyDelayTime());
//          }

/*
            if (rawAdcReading[currentChannel] != 0xFFFFFF)
            {
                dbgSerial.print("\nValue=");
                dbgSerial.println( rawAdcReading[currentChannel], HEX);
            }
*/
            if (0  &&  currentChannel == 0)
            {
                  //   Dump first three channels
                  dbgSerial.print(rawAdcReading[0], DEC);
                  dbgSerial.print(",");
                  dbgSerial.print(rawAdcReading[1], DEC);
                  dbgSerial.print(",");
                  dbgSerial.print(rawAdcReading[2], DEC);
                  dbgSerial.println();
            }

//          delay(2);
//          adcConverter.ads1256_Reg_Read(0, readBuff, 6);

            //   Send the latest measurement off to the measurement manager
            switch (currentChannel)
            {
            case 0:
                sendEvent(msmtMgrEvt_ReportRawSensorMsmt, rawAdcReading[currentChannel], sensorType_DistanceLength);
                break;

            case 1:
                sendEvent(msmtMgrEvt_ReportRawSensorMsmt, rawAdcReading[currentChannel], sensorType_DistanceWidth);
                break;

            case 2:
                sendEvent(msmtMgrEvt_ReportRawSensorMsmt, rawAdcReading[currentChannel], sensorType_DistanceHeight);
                break;

            case 3:
            case 4:
            default:
                //  Nothing to do now with this channel
                break;
            }
            cancelTimer(adcCvtEvt_WatchdogTimeout);
            sendEvent(timerEvt_startTimer, adcCvtEvt_WatchdogTimeout, 1000);
        }
        else
        {
//          digitalWrite(10, HIGH);
//          digitalWrite(10, LOW );
//          adcConverter.addToFirstDrdyDelayTime(1);

            /*  Wait another millisecond and try again  */
            sendEvent(timerEvt_cancelTimer, adcCvtEvt_WaitConverterReadyTimeout, 0);
            sendEvent(timerEvt_startTimer,  adcCvtEvt_WaitConverterReadyTimeout, 1);
        }
        break;

    case adcCvtEvt_RestartCurrentConversionTimeout:
        //   This will restart the conversion, if it is useful for debug to delay conversion for testin
        adcConverter.ads1256_RestartCurrentMeasurement();
        sendEvent(timerEvt_cancelTimer, adcCvtEvt_WaitConverterReadyTimeout, 0);
        sendEvent(timerEvt_startTimer,  adcCvtEvt_WaitConverterReadyTimeout, 12);
        break;

        default:
            break;
    }
}
