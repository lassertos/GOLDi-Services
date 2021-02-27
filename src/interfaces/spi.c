#include "spi.h"
#include <string.h>
#include "../logging/log.h"

int setupSPIInterface()
{
    if (!bcm2835_init())
    {
        return 1;
    }
    if (!bcm2835_spi_begin())
    {
        return 1;
    }
    bcm2835_spi_set_speed_hz(3900000);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, 0);
    return 0;
}

void closeSPIInterface()
{
    bcm2835_spi_end();
    bcm2835_close();
}

spiCommand SensorTypeToSPICommandRead(SensorType sensorType)
{
    switch (sensorType)
    {
        case SensorTypeBinary:
            return SPICOMMAND_READ_BINARY;
        
        default:
            log_error("SPI: unknown SensorType");
            return (spiCommand){0x00, 0, 0};
    }
}

spiCommand ActuatorTypeToSPICommandRead(ActuatorType actuatorType)
{
    switch (actuatorType)
    {
        case ActuatorTypeBinary:
            return SPICOMMAND_READ_BINARY;
        
        default:
            log_error("SPI: unknown ActuatorType");
            return (spiCommand){0x00, 0, 0};
    }
}

spiCommand SensorTypeToSPICommandWrite(SensorType sensorType)
{
    switch (sensorType)
    {
        case SensorTypeBinary:
            return SPICOMMAND_WRITE_BINARY;
        
        default:
            log_error("SPI: unknown SensorType");
            return (spiCommand){0x00, 0, 0};
    }
}

spiCommand ActuatorTypeToSPICommandWrite(ActuatorType actuatorType)
{
    switch (actuatorType)
    {
        case ActuatorTypeBinary:
            return SPICOMMAND_WRITE_BINARY;
        
        default:
            log_error("SPI: unknown ActuatorType");
            return (spiCommand){0x00, 0, 0};
    }
}

int executeSPICommand(spiCommand command, unsigned char pinMapping, char* value, pthread_mutex_t* mutex)
{
    int completeCommandLength = 1 + command.dataLength + command.answerLength;
    char* completeCommand = malloc(completeCommandLength);
    if (completeCommand == NULL)
    {
        log_error("SPI: error occurred during malloc");
        return -1;
    }

    completeCommand[0] = command.command;
    completeCommand[1] = pinMapping;
    memcpy(completeCommand + 2, value, command.dataLength-1);
    for (int i = 0; i < command.answerLength; i++)
    {
        completeCommand[1 + command.dataLength + i] = 0;
    }

    pthread_mutex_lock(mutex);
    bcm2835_spi_transfern(completeCommand, completeCommandLength);
    pthread_mutex_unlock(mutex);

    if (command.answerLength > 0)
    {
        for (int i = 0; i < command.answerLength; i++)
        {
            value[i] = completeCommand[completeCommandLength - command.answerLength + i];
        }
    }
    free(completeCommand);
    return 0;
}


int SPIReadSensor(Sensor* sensor, pthread_mutex_t* mutex)
{
    spiCommand command = SensorTypeToSPICommandRead(sensor->type);
    return executeSPICommand(command, sensor->pinMapping, sensor->value, mutex);
}

int SPIReadActuator(Actuator* actuator, pthread_mutex_t* mutex)
{
    spiCommand command = ActuatorTypeToSPICommandRead(actuator->type);
    return executeSPICommand(command, actuator->pinMapping, actuator->value, mutex);
}

int SPIWriteSensor(Sensor* sensor, pthread_mutex_t* mutex)
{
    spiCommand command = SensorTypeToSPICommandWrite(sensor->type);
    return executeSPICommand(command, sensor->pinMapping, sensor->value, mutex);
}

int SPIWriteActuator(Actuator* actuator, pthread_mutex_t* mutex)
{
    spiCommand command = ActuatorTypeToSPICommandWrite(actuator->type);
    return executeSPICommand(command, actuator->pinMapping, actuator->value, mutex);
}
