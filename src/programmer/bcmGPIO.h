#include <bcm2835.h>
#include <stdio.h>

#define TCK 25
#define TMS 27
#define TDO 24
#define TDI 26

int initGPIO(void);
void writeGPIO(int pin, unsigned data);
unsigned readGPIO(int pin);
void stopGPIO(void);
