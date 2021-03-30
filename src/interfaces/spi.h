#ifndef SPI_H
#define SPI_H

#define SPICOMMAND_READ_BINARY  (spiCommand){0xFC, 1, 1}
#define SPICOMMAND_WRITE_BINARY (spiCommand){0xFD, 2, 0}

//TODO give the correct command to the sensors and actuators with the config data

#include <bcm2835.h>
#include <stdio.h>
#include <pthread.h>
#include "SensorsActuators.h"

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
int SPIReadSensor(Sensor* sensor, pthread_mutex_t* mutex);
int SPIReadActuator(Actuator* actuator, pthread_mutex_t* mutex);
int SPIWriteSensor(Sensor* sensor, pthread_mutex_t* mutex);
int SPIWriteActuator(Actuator* actuator, pthread_mutex_t* mutex);


#endif