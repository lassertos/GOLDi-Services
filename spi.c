#include "spi.h"

int setupSPIInterface()
{
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