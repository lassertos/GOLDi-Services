#include "interfaces/ipcsockets.h"
#include "interfaces/websockets.h"
#include "utils/utils.h"
#include "parsers/json.h"

// TODO maybe add explanation for variables
/* global variables needed for execution */
static websocketConnection wscLabserver;
static websocketConnection wscPhysicalSystem;
static IPCSocketConnection* commandService;
static IPCSocketConnection* programmingService;
static JSON* deviceDataJSON;
static char* deviceData;
static JSON* experimentInitAck;

static void sigint_handler(int sig)
{
	wscLabserver.interrupted = 1;
    pthread_join(wscLabserver.thread, NULL);
    wscPhysicalSystem.interrupted = 1;
    pthread_join(wscPhysicalSystem.thread, NULL);
    if(commandService && commandService->open)
        closeIPCConnection(commandService);
    if(programmingService && programmingService->open)
        closeIPCConnection(programmingService);
    exit(0);
}

static int handleWebsocketMessage(struct lws* wsi, char* message)
{
    int result = 0;
    JSON* msgJSON = JSONParse(message);
    JSON* msgCommand = JSONGetObjectItem(msgJSON, "Command");

    switch (msgCommand->valueint)
    {
        /* is sent by labserver to indicate that the device was registered successfully */
        case WebsocketCommandDeviceRegistered:
            //TODO think about what should happen here, maybe just log?
            break;

        /* initialize the Command Service with the received experiment data (after initialization send ExperimentInitAck) */
        case WebsocketCommandExperimentData:
            JSONDeleteItemFromObject(msgJSON, "Command");
            JSONDeleteItemFromObject(msgJSON, "SenderID");
            char* experimentData = JSONPrint(msgJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_INITCOMMANDSERVICE, experimentData, strlen(experimentData));
            free(experimentData);
            break;

        /* start init and wait for experiment data */
        case WebsocketCommandExperimentInit:
            //TODO maybe save that experiment is currently being initialized 
            /* initializing experiment init ack for later use */
            int experimentID = JSONGetObjectItem(JSONGetObjectItem(msgJSON, "data"), "ExperimentID");
            unsigned int virtualPartner = JSONIsTrue(JSONGetObjectItem(msgJSON, "virtualPartner"));
            experimentInitAck = JSONCreateObject();
            JSONAddNumberToObject(experimentInitAck, "SenderID", JSONGetObjectItem(deviceDataJSON, "DeviceID")->valueint);
            JSONAddNumberToObject(experimentInitAck, "Command", WebsocketCommandExperimentInitAck);
            JSONAddNumberToObject(experimentInitAck, "ExperimentID", experimentID);
            if (virtualPartner == 0)
            {
                JSONAddFalseToObject(experimentInitAck, "virtualPartner");
            }
            else
            {
                JSONAddTrueToObject(experimentInitAck, "virtualPartner");
            }
            break;

        /* close connection to physical system if created, stop sending data and send ExperimentCloseAck */
        case WebsocketCommandExperimentClose:
            wscPhysicalSystem.interrupted = 1;
            pthread_join(wscPhysicalSystem.thread, NULL);
            JSON* experimentCloseAckJSON = JSONCreateObject();
            JSONAddNumberToObject(experimentCloseAckJSON, "Command", WebsocketCommandExperimentCloseAck);
            char* experimentCloseAck = JSONPrint(experimentCloseAckJSON);
            sendMessageWebsocket(wsi, experimentCloseAck);
            sendMessageIPC(commandService, IPCMSGTYPE_ENDEXPERIMENT, NULL, 0);
            JSONDelete(experimentCloseAckJSON);
            free(experimentCloseAck);
            break;

        /* sent by the Physical System if a delay fault has been detected */
        case WebsocketCommandDelayFault:
            JSONDeleteItemFromObject(msgJSON, "SenderID");
            JSONAddNumberToObject(msgJSON, "SenderID", JSONGetObjectItem(deviceDataJSON, "DeviceID")->valueint);
            char* stringDelayFault = JSONPrint(msgJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_DELAYBASEDFAULT, stringDelayFault, strlen(stringDelayFault));
            free(stringDelayFault);
            break;

        /* used to reset the control unit */
        case WebsocketCommandResetCU:
            //TODO add implementation
            break;

        /* forward to Command Service */
        case WebsocketCommandSensorData:
            JSON* sensorDataJSON = JSONGetObjectItem(msgJSON, "SensorData");
            char* sensorData = JSONPrint(sensorDataJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_SENSORDATA, sensorData, strlen(sensorData));
            free(sensorData);
            break;
        
        /* used to send the programming file as a base64 encoded string, decode, send to Programming Service and send ack*/
        case WebsocketCommandProgramCU:
            //TODO decode string and save to file, send programming command to Programming Service
            char* programData = decodeBase64(JSONGetObjectItem(msgJSON, "File")->valuestring);
            FILE *write_ptr = fopen("test.hex","wb");
            fwrite(programData, strlen(programData),1,write_ptr);
            sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0);
            sendMessageIPC(commandService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0); //TODO really needed?
            break;

        /* try to connect to Physical System and send ack with outcome accordingly */
        case WebsocketCommandDirectConnectionInit:
            JSON* directConnectionAckJSON = JSONCreateObject();
            JSONAddNumberToObject(directConnectionAckJSON, "Command", WebsocketCommandDirectConnectionAck);
            char* subnetPS = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "Subnet")->valuestring;
            char* ipAdressPS = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "IP")->valuestring;
            char* subnetCU = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "Subnet")->valuestring;
            if (!strcmp(subnetPS, subnetCU)) 
            {
                if(websocketPrepareContext(&wscPhysicalSystem, COMMUNICATION_PROTOCOL, ipAdressPS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
                {
                    //fail
                    JSONAddFalseToObject(directConnectionAckJSON, "Outcome");
                }
                else
                {
                    //success
                    JSONAddTrueToObject(directConnectionAckJSON, "Outcome");
                }
            }
            char* directConnectionAck = JSONPrint(directConnectionAckJSON);
            sendMessageWebsocket(wscLabserver.wsi, directConnectionAck);
            JSONDelete(directConnectionAckJSON);
            free(directConnectionAck);
            break;
            
        default:
            result = -1;
            break;
    }

    free(message);
    cJSON_Delete(msgJSON);
    return result;
}

static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            if(hasMessages(ipcsc))
            {
                Message msg = receiveMessageIPC(ipcsc);
                printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_INITCOMMANDSERVICEFINISHED:
                        if (msg.length == 0)
                        {
                            // an error has occured TODO error handling
                        }
                        else
                        {
                            JSON* msgJSON = JSONParse(msg.content);
                            JSON* actuatorDataJSON = JSONGetObjectItem(msgJSON, "ActuatorData");
                            JSONAddNullToObject(experimentInitAck, "Experiment");
                            JSONAddNullToObject(experimentInitAck, "SensorData");
                            JSONAddItemToObject(experimentInitAck, "ActuatorData", actuatorDataJSON);
                            char* message = JSONPrint(experimentInitAck);
                            sendMessageWebsocket(wscLabserver.wsi, message);
                            JSONDelete(msgJSON);
                            JSONDelete(experimentInitAck);
                            free(message);
                        }
                        break;

                    case IPCMSGTYPE_DELAYBASEDFAULTACK:
                        if (wscPhysicalSystem.connectionEstablished && !wscPhysicalSystem.interrupted)
                        {
                            sendMessageWebsocket(wscPhysicalSystem.wsi, msg.content);
                        }
                        else
                        {
                            sendMessageWebsocket(wscLabserver.wsi, msg.content);
                        }
                        break;

                    case IPCMSGTYPE_INTERRUPTED:
                        ipcsc->open = 0;
                        closeIPCConnection(ipcsc);
                        free(msg.content); 
                        return -1;
                        break;

                    case IPCMSGTYPE_CLOSEDCONNECTION:
                        ipcsc->open = 0;
                        closeIPCConnection(ipcsc);
                        free(msg.content); 
                        return 0;
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
    signal(SIGINT, sigint_handler);

    /* create all needed sockets */
    if(websocketPrepareContext(&wscLabserver, COMMUNICATION_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wscLabserver.connectionEstablished);

    commandService = connectToIPCSocket(COMMAND_SERVICE, messageHandlerIPC);
    if (commandService == NULL)
        return -1;

    programmingService = connectToIPCSocket(PROGRAMMING_SERVICE, messageHandlerIPC);
    if (programmingService == NULL)
        return -1;

    pthread_join(wscLabserver.thread, NULL);
    pthread_join(commandService->thread, NULL);
    pthread_join(programmingService->thread, NULL);

    sigint_handler(0);

    return 0;
}