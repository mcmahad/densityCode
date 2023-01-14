

#ifndef     _WIN32SHIMS_H
#define     _WIN32SHIMS_H

#ifdef _WIN32
#include <stdio.h>
#include <stdint.h>

#define F(A)        A
#define strcpy_P    strcpy
#define strlen_P    strlen
#define PSTR(A)     A
#define PGM_P(A)

const   int     HEX = 1,
                DEC = 2,
                OUTPUT = 3,
                LOW = 5,
                HIGH = 6;

class HardwareSerial
{
public:
    void print(const char* string) { printf("%s", string); }
    void print(      char* string) { printf("%s", string); }
    void print(uint32_t value) { printf("%ul", (long)value); }
    void print(const int     value) { printf("%d",  value); }
    void println(void) { printf("\n"); }
    void println(const int16_t value) { printf("%d\n", value); }
    void println(const int     value) { printf("%d\n", value); }
    void println(const char* string) { printf("%s\n", string); }
    void println(double   value, int base) { printf("%lf\n", value); }
    void println(uint32_t value) { printf("%u\n", value); }
    void println(double   value) { printf("%lf\n", value); }
    void print(double   value, int base) { printf("%lf", value); }
    void print(double   value) { printf("%lf", value); }
    void print( int32_t value, int base) { printf("%d", value); }
    void begin(int baudRate) {}
};

class EEPROM_t
{
public:
    inline uint8_t read (int address) { return 0; };
//  inline uint8_t write(int address, int value) { return 0; };
    inline uint8_t write(int address, uint8_t newValue) { return 0; };
};



static inline void pinMode(int pinNum, int direction) { }

static inline void delay(int amt) { }

/*  This is external, definition found in tareSim.cpp
*/
uint32_t millis(void);

static inline void digitalWrite(int way, int pin) { }

#define     min(A, B)       ((A < B) ? A : B)
#define     max(A, B)       ((A > B) ? A : B)

#define     _NOP()

void dtostrf(double fltVal, int length, int decimalCnt, char const* destStr);

#endif  //  _WIN32


#endif  //   _WIN32SHIMS_H


