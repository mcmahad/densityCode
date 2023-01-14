
#ifdef   _WIN32
#include "win32shims.h"


EEPROM_t  EEPROM;

void dtostrf(double fltVal, int length, int decimalCnt, char const * destStr) {}

#endif

