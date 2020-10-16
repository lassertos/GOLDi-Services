#include "spi.h"

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
}

spiAnswer* executeSPICommand(spiCommand command)
{
    spiAnswer* answer = malloc(answer);
    int completeCommandLength = command.commandLength + command.dataLength + command.answerLength;
    char* completeCommand = malloc(completeCommandLength);
    if (completeCommand == NULL)
    {
        return NULL;
    }

    memcpy(completeCommand, command.command, command.commandLength);
    memcpy(completeCommand + command.commandLength, command.data, command.dataLength);
    for (int i = 0; i < command.answerLength; i++)
    {
        completeCommand[command.commandLength + command.dataLength + i] = 0;
    }

    for (int i = 0; i < completeCommandLength - command.answerLength; i++)
    {  
        printf("SPICOMMAND: %x\n", completeCommand[i] & 0xff);
    }
    bcm2835_spi_transfern(completeCommand, completeCommandLength);
    for (int i = 0; i < command.answerLength; i++)
    {  
        printf("SPIANSWER: %x\n", completeCommand[i] & 0xff);
    }

    if (command.answerLength > 0)
    {
        answer->answer = realloc(completeCommand, command.answerLength);
    }
    else 
    {
        answer->answer = NULL;
        free(completeCommand);
    }    

    answer->answerLength = command.answerLength;

    return answer;
}