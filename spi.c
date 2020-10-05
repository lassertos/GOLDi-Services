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

void executeSPICommand(unsigned char* command, int length)
{
    for (int i = 0; i < length; i++)
    {  
        printf("SPICOMMAND: %x\n", command[i] & 0xff);
    }
    bcm2835_spi_transfern(command, length);
    for (int i = 0; i < length; i++)
    {  
        printf("SPIANSWER: %x\n", command[i] & 0xff);
    }
}