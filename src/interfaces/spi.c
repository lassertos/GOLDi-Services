#include "spi.h"
#include <string.h>

int setupSPIInterface()
{
    if (!bcm2835_init())
    {
        return -1;
    }
    if (!bcm2835_spi_begin())
    {
        return -1;
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

// TODO: look if it needs a revamp and add return value for failure
spiAnswer executeSPICommand(spiCommand command, char* data, pthread_mutex_t* mutex)
{
    spiAnswer answer = {NULL, 0};
    int completeCommandLength = 1 + command.dataLength + command.answerLength;
    char* completeCommand = malloc(completeCommandLength);
    if (completeCommand == NULL)
    {
        return answer;
    }

    completeCommand[0] = command.command;
    memcpy(completeCommand + 1, data, command.dataLength);
    for (int i = 0; i < command.dataLength; i++)
    {
        completeCommand[1 + command.dataLength + i] = 0;
    }

    printf("SPICOMMAND: ");
    for (int i = 0; i < completeCommandLength - command.answerLength; i++)
    {  
        printf("%x", completeCommand[i] & 0xff);
    }
    printf("\n");

    pthread_mutex_lock(mutex);
    bcm2835_spi_transfern(completeCommand, completeCommandLength);
    pthread_mutex_unlock(mutex);

    printf("SPIANSWER: ");
    for (int i = 0; i < command.answerLength; i++)
    {  
        printf("%x", completeCommand[i] & 0xff);
    }
    printf("\n");

    //TODO check if it really works like that
    if (command.answerLength > 0)
    {
        answer.content = malloc(command.answerLength);
        for (int i = 0; i < command.answerLength; i++)
        {
            answer.content[i] = completeCommand[completeCommandLength - command.answerLength + i];
        }
        free(completeCommand);
    }
    else 
    {
        answer.content = NULL;
        free(completeCommand);
    }    

    answer.length = command.answerLength;

    return answer;
}