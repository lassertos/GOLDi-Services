#ifndef SPI_H
#define SPI_H

#include <bcm2835.h>
#include <stdio.h>
#include <pthread.h>

typedef struct spiCommand_s
{
    char            command;
    unsigned int    dataLength;
    unsigned int    answerLength;
} spiCommand;

typedef struct spiAnswer_s
{
    char*           content;
    unsigned int    length;
} spiAnswer;


int setupSPIInterface();
void closeSPIInterface();
spiAnswer executeSPICommand(spiCommand command, char* data, pthread_mutex_t* mutex);

#endif