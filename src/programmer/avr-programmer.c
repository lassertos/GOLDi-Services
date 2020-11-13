#include <stdlib.h>
#include <string.h>

//TODO change reset pin from 25 to sth else in avrdude.conf

int programControlUnitMicrocontroller(char* filepath)
{
    char* command = "avrdude -c linuxspi -p m2560 -P /dev/spidev0.1 -U flash:w:";
    char* completeCommand = malloc(strlen(command) + strlen(filepath) + 1);

    strncpy(completeCommand, command, strlen(command));
    strncpy(completeCommand+strlen(command), filepath, strlen(filepath));
    completeCommand[strlen(command) + strlen(filepath)] = '\0';

    int result = system(completeCommand);
    free(completeCommand);

    return result;
}