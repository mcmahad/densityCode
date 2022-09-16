/**
 *
 * ADS1232 library for Arduino
 *
 *
**/
#ifndef ADS1232_h
#define ADS1232_h

#include <stdint.h>


#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


class ADS1232
{
	private:
        bool calibrateOffsetNeeded;
		uint8_t PD_SCK;	// Power Down and Serial Clock Input Pin
		uint8_t DOUT;		// Serial Data Output Pin
		uint8_t PDWN;		// powerDown Output Pin
        uint8_t filterScaling_Pctg;     //  range = 0..100 percent
        uint8_t sensorId;
        uint8_t nextFirstDelay;

	public:

long    rawMsmt_cnts,
        filteredAvg_cnts,
        tareValue_cnts,

        //  This allows only a single point calibration and isn't very accurate
        scaling_CountsPerGram_x1000;

		ADS1232();

		virtual ~ADS1232();

		// Initialize library with data output pin, clock input pin
		// -
		void begin(byte dout, byte pd_sck, byte pdwn, uint8_t sensorId);

        long read();

        void    setFilterWeightPctg(byte newPctgValue) {filterScaling_Pctg = newPctgValue;}

        inline long getFilteredValue() {return filteredAvg_cnts;}
        inline long getLastRawValue () {return rawMsmt_cnts;    }

		// Check if ADS1232 is ready
		// from the datasheet: When output data is not ready for retrieval, digital output pin DOUT is high. Serial clock
		// input PD_SCK should be low. When DOUT goes to low, it indicates data is ready for retrieval.
		bool is_ready();

		// puts the chip into power down mode
		void power_down();

		// wakes up the chip after power down mode
		bool power_up();

        //  Toggle SCLK once
        void makeSclkPulse();

        //  Set the current tare using filtered measurements
        inline void set_tare() {tareValue_cnts = filteredAvg_cnts;}

        inline long get_tare() {return tareValue_cnts;}

        //  Set the current scale using filtered measurements
        void set_scale(int currentWeight_grams);

        inline void set_FirstDelay(uint8_t newDelay)
        {
            if (newDelay <  90) newDelay =  90;
            if (newDelay > 110) newDelay = 110;
            nextFirstDelay = newDelay;
        };
        inline uint8_t get_FirstDelay() { return nextFirstDelay; };

        //  Cause an internal hardware calibration for the ADC chip
        inline void calibrate_offset() {calibrateOffsetNeeded = true;}

        //  Return the ID for this sensor
        inline uint8_t get_sensorId() {return sensorId;}

        int get_ScaledFilteredWeight();
};

#endif /* ADS1232_h */

