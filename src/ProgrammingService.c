#include "interfaces/ipcsockets.h"
#include "programmer/goldi-programmer.h"

IPCSocketConnection* communicationService;

enum ControlUnitTypes
{
    CUTYPE_MICROCONTROLLER,
    CUTYPE_PLD
} cuType;

static int messageHandlerCommunicationService(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            if(hasMessages(ipcsc))
            {
                Message msg = receiveMessageIPC(ipcsc);
                //printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_PROGRAMFPGA:
                    {
                        programFPGA(msg.content);
                        break;
                    }

                    case IPCMSGTYPE_PROGRAMCONTROLUNIT:
                    {
                        int result = 1;
                        switch(cuType)
                        {
                            case CUTYPE_MICROCONTROLLER:
                                programControlUnitMicrocontroller("MicroController.hex");
                                break;
                            case CUTYPE_PLD:
                                programFPGA("PLD.pof");
                                break;
                            default:
                                break;
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITPROGRAMMINGSERVICE:
                    {
                        int result = 0;
                        if (!strcmp(msg.content, "MicroController"))
                        {
                            cuType = CUTYPE_MICROCONTROLLER;
                            result = 1;
                        }
                        else if (!strcmp(msg.content, "ProgrammableLogicDevice"))
                        { 
                            cuType = CUTYPE_PLD;
                            result = 1;
                        }
                        char* resultString = serializeInt(result);
                        sendMessageIPC(communicationService, IPCMSGTYPE_INITPROGRAMMINGSERVICEFINISHED, resultString, 4);
                        free(resultString);
                        break;
                    }

                    case IPCMSGTYPE_INTERRUPTED:
                    {
                        ipcsc->open = 0;
                        closeIPCConnection(ipcsc);
                        free(msg.content); 
                        return -1;
                        break;
                    }

                    case IPCMSGTYPE_CLOSEDCONNECTION:
                    {
                        ipcsc->open = 0;
                        closeIPCConnection(ipcsc);
                        free(msg.content); 
                        return 0;
                        break;
                    }

                    default:
                    {
                        break;
                    }
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
    int fd = createIPCSocket(PROGRAMMING_SERVICE);
    communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
        return -1;
    
    pthread_join(communicationService->thread, NULL);

    return 0;
}