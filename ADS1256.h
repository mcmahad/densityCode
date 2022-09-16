////////////////////////////////////////////////////////////////////////////////////////////
//    Arduino library for the ADS1256 32-bit ADC
//    Copyright (c) 2021 Dave McMahan
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef ads1256_h
#define ads1256_h

#include <stdint.h>
#include "Arduino.h"

/*  ACTIVE_ADC_CHANNEL_CNT sets number of channels to scan, starting at
    channel 0.   This value should be between 1 and 8
*/
#define     ACTIVE_ADC_CHANNEL_CNT      3



class ads1256
{
private:
        uint8_t     spiChipSel_pin,
                    dataReady_pin,
                    syncDown_pin,
                    reset_pin,
                    mosi_pin,
                    miso_pin,
                    spiclk_pin,
                    currentChannel;
        uint16_t    drdyTimeDelay_mSec;

public:
    void    ads1256_Init(uint8_t spiChipSelPin, uint8_t dataReadyPin, uint8_t syncPdwnPin, uint8_t resetPin);
    void    ads1256_Reg_Write (uint8_t readWriteAddress, uint8_t *srcDataPtr,  uint8_t dataLength, bool raiseChipSelAtEnd);
    void    ads1256_Reg_Read  (uint8_t readWriteAddress, uint8_t *destDataPtr, uint8_t dataLength);
    void    addToFirstDrdyDelayTime (int adjValue);
    int     getFirstDrdyDelayTime   (void);
    int     getCurrentChannelNumber (void);
    bool    is_DataReady            (void);
    int32_t ads1256_ReadData        (void);
    int32_t readConvertedDataAndStartNextChannel(void);
    void    ads1256_SetNewChannelGain(int32_t newGain);
    void    ads1256_SetNewChannelOffset(uint32_t newOffset);
    void    ads1256_RestartCurrentMeasurement(void);
    void    SPI_transfer(uint8_t value);
    uint8_t SPI_transfer_read(void);
};


#endif  //  ads1256_h
