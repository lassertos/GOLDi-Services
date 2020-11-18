#ifndef SPI_H
#define SPI_H

#define SPICOMMAND_READ_GPIO  (spiCommand){0xFC, 1, 1}
#define SPICOMMAND_WRITE_GPIO (spiCommand){0xFD, 2, 0}

//TODO give the correct command to the sensors and actuators with the config data

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