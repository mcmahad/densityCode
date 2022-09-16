  
extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;

#include "events.h"


void setup()
{
    // put your setup code here, to run once:
    initializeEvents();
}

void loop()
{
    // put your main code here, to run repeatedly:
    processEvents();
}
