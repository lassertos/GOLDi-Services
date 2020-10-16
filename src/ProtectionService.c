#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"

static IPCSocketConnection* communicationService;
Sensor* sensors;
Actuator* actuators;

static int messageHandlerCommunicationService(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            if(hasMessages(ipcsc))
            {
                Message msg = receiveMessageIPC(ipcsc);
                printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                char* spiAnswer;
                switch (msg.type)
                {
                case IPCMSGTYPE_INTERRUPTED:
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    return -1;
                    break;

                case IPCMSGTYPE_CLOSEDCONNECTION:
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    return 0;
                    break;

                case IPCMSGTYPE_SPICOMMAND:
                    spiAnswer = malloc(msg.length);
                    memcpy(spiAnswer, msg.content, msg.length);
                    //executeSPICommand(spiAnswer, msg.length);
                    sendMessageIPC(ipcsc, IPCMSGTYPE_SPIANSWER, spiAnswer, msg.length);
                    free(spiAnswer);
                    break;
                
                default:
                    break;
                }
                free(msg.content);
            }
        }
        else
        {
            break;
        }
    }
}

int main(int argc, char const *argv[])
{

    int fd = createIPCSocket(PROTECTION_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    while(1)
    {
        /* Read all IPC Messages and update CurrentActuator */
        while (hasMessages(communicationService));
    
        /* Save the Sensorvalues in CurrentSensor */

        /* Check the Protection rules */

        /* Send the CurrentActuator values to the FPGA */

    }

    pthread_join(communicationService->thread, NULL);

    return 0;
}
