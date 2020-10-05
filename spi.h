#ifndef SPI_H
#define SPI_H

#include <bcm2835.h>
#include <stdio.h>

int setupSPIInterface();
void closeSPIInterface();
void executeSPICommand(unsigned char* command, int length);

#endif