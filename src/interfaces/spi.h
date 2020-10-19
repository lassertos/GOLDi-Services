#ifndef SPI_H
#define SPI_H

#include <bcm2835.h>
#include <stdio.h>

typedef struct spiCommand_s
{
    char*   command;
    int     commandLength;
    int     dataLength;
    int     answerLength;
} spiCommand;

typedef struct spiAnswer_s
{
    char*   answer;
    int     answerLength;
} spiAnswer;


int setupSPIInterface();
void closeSPIInterface();
spiAnswer executeSPICommand(spiCommand command, char* data);

#endif