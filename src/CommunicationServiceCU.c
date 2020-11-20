#define FPGASVF_FILENAME "FPGA.svf"

#include "interfaces/ipcsockets.h"
#include "interfaces/websockets.h"
#include "utils/utils.h"
#include "parsers/json.h"
#include "logging/log.h"

//TODO add signal handler for updates

/* global variables needed for execution */
static websocketConnection wscLabserver;                // the websocket to the Labserver
static websocketConnection wscPhysicalSystem;           // the websocket to the Physical System
static IPCSocketConnection* commandService;             // the IPC-socket to the Command Service
static IPCSocketConnection* programmingService;         // the IPC-socket to the Programming Service
static JSON* deviceDataJSON;                            // here the DeviceData is kept in JSON-format
static char* deviceData;                                // here the DeviceData is kept as a string
static unsigned int deviceID;                           // here we save the DeviceID
static JSON* experimentInitAck;                         // this is needed to be able to delay the sending of the Ack
static volatile int initializedProgrammingService = 0;  // indicates whether the ProgrammingService has been initialized successfully 

/*
 * the sigint handler, can also be used for cleanup after execution 
 * sig - the signal the program received, can be ignored for our purposes
 */
static void sigint_handler(int sig)
{
    JSONDelete(deviceDataJSON);
    free(deviceData);
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
        {
            //TODO think about what should happen here, maybe just log?
            log_info("device has been registered successfully");
            break;
        }

        /* initialize the Command Service with the received experiment data (after initialization send ExperimentInitAck) */
        case WebsocketCommandExperimentData:
        {
            JSONDeleteItemFromObject(msgJSON, "Command");
            JSONDeleteItemFromObject(msgJSON, "SenderID");
            char* experimentData = JSONPrint(msgJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_INITCOMMANDSERVICE, experimentData, strlen(experimentData));
            free(experimentData);
            break;
        }

        /* start init and wait for experiment data */
        case WebsocketCommandExperimentInit:
        {
            /* initializing experiment init ack for later use (when command service initialization finished) */
            int experimentID = JSONGetObjectItem(JSONGetObjectItem(msgJSON, "data"), "ExperimentID")->valueint;
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
        }

        /* close connection to physical system if created, stop sending data and send ExperimentCloseAck */
        case WebsocketCommandExperimentClose:
        {
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
        }

        /* sent by the Physical System if a delay fault has been detected */
        case WebsocketCommandDelayFault:
        {
            JSONDeleteItemFromObject(msgJSON, "SenderID");
            JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
            JSONDeleteItemFromObject(msgJSON, "Command");
            JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandDelayFaultAck);
            char* stringDelayFault = JSONPrint(msgJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_DELAYBASEDFAULT, stringDelayFault, strlen(stringDelayFault));
            free(stringDelayFault);
            break;
        }

        /* used to reset the control unit */
        case WebsocketCommandResetCU:
        {
            //TODO add implementation
            break;
        }

        /* forward to Command Service */
        case WebsocketCommandSensorData:
        {
            JSON* sensorDataJSON = JSONGetObjectItem(msgJSON, "SensorData");
            char* sensorData = JSONPrint(sensorDataJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_SENSORDATA, sensorData, strlen(sensorData));
            free(sensorData);
            break;
        }
        
        /* used to send the programming file as a base64 encoded string, decode, send to Programming Service and send ack*/
        case WebsocketCommandProgramCU:
        {
            unsigned int length = 0;
            char* programData = decodeBase64(JSONGetObjectItem(msgJSON, "File")->valuestring, &length);
            FILE *write_ptr = fopen("/tmp/GOLDiServices/ProgrammingService/programmingfile","wb");
            fwrite(programData, 1, length, write_ptr);
            sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0);
            sendMessageIPC(commandService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0);
            break;
        }

        /* try to connect to Physical System and send ack with outcome accordingly */
        case WebsocketCommandDirectConnectionInit:
        {
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
        }
            
        default:
        {
            result = -1;
            break;
        }
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
                //log_debug("\nMESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_ACTUATORDATA:
                    {
                        JSON* msgJSON = JSONParse(msg.content);

                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandActuatorData);

                        char* message = JSONPrint(msgJSON);
                        if (wscPhysicalSystem.connectionEstablished && !wscPhysicalSystem.interrupted)
                        {
                            sendMessageWebsocket(wscPhysicalSystem.wsi, message);
                        }
                        else
                        {
                            sendMessageWebsocket(wscLabserver.wsi, message);
                        }

                        free(message);
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_INITCOMMANDSERVICEFINISHED:
                    {
                        if (msg.length == 0)
                        {
                            // an error has occured TODO error handling
                            log_error("the Command Service could not be initialized correctly");
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
                    }

                    case IPCMSGTYPE_INITPROGRAMMINGSERVICEFINISHED:
                    {
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            initializedProgrammingService = -1;
                        }
                        else
                        {
                            initializedProgrammingService = 1;
                        }
                        break;
                    }

                    case IPCMSGTYPE_DELAYBASEDFAULTACK:
                    {
                        if (wscPhysicalSystem.connectionEstablished && !wscPhysicalSystem.interrupted)
                        {
                            sendMessageWebsocket(wscPhysicalSystem.wsi, msg.content);
                        }
                        else
                        {
                            sendMessageWebsocket(wscLabserver.wsi, msg.content);
                        }
                        break;
                    }

                    case IPCMSGTYPE_PROGRAMCONTROLUNITFINISHED:
                    {
                        //TODO check that result values of programming functions are the same, seems like 0 = success
                        int result = deserializeInt(msg.content);
                        sendMessageIPC(commandService, IPCMSGTYPE_PROGRAMCONTROLUNITFINISHED, NULL, 0); //send only if programming successful
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
    signal(SIGINT, sigint_handler);

    /* create all needed sockets */
    if(websocketPrepareContext(&wscLabserver, COMMUNICATION_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wscLabserver.connectionEstablished);

    commandService = connectToIPCSocket(COMMAND_SERVICE, messageHandlerIPC);
    if (commandService == NULL)
    {
        return -1;
    }

    programmingService = connectToIPCSocket(PROGRAMMING_SERVICE, messageHandlerIPC);
    if (programmingService == NULL)
    {
        return -1;
    }

    char* deviceConfigContent = readFile("/data/GOLDiServices/DeviceData.json", NULL);
    deviceDataJSON = JSONParse(deviceConfigContent);
    free(deviceConfigContent);
    deviceID = JSONGetObjectItem(deviceDataJSON, "DeviceID")->valueint;

    JSON* jsonExperimentType = JSONGetObjectItem(deviceDataJSON, "ExperimentType");
    sendMessageIPC(programmingService, IPCMSGTYPE_INITPROGRAMMINGSERVICE, jsonExperimentType->valuestring, strlen(jsonExperimentType->valuestring));

    while (!initializedProgrammingService > 0)
    {
        if (initializedProgrammingService == -1)
        {
            //TODO error handling
            log_error("the Programming Service could not be initialized correctly");
            return -1;
        }
    }

    /* find fpga programming file and read content */ 
    //TODO rename experiment -> control unit
    char* experimentType = jsonExperimentType->valuestring;
    char* experimentPath = "/etc/GOLDiServices/controlunits/";
    char* fpgaSVFPath = malloc(strlen(experimentPath) + strlen(experimentType) + strlen(FPGASVF_FILENAME) + 2);
    strcpy(fpgaSVFPath, "/etc/GOLDiServices/controlunits/");    //TODO create directory with yocto and place files
    strcat(fpgaSVFPath, experimentType);
    strcat(fpgaSVFPath, "/");
    strcat(fpgaSVFPath, FPGASVF_FILENAME);
    char* fpgaSVFContent = readFile(fpgaSVFPath, NULL);
    if (fpgaSVFContent == NULL)
    {
        log_error("something went wrong while trying to read the fpga programming file");
    }
    free(fpgaSVFPath);

    /* program the fpga */
    sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMFPGA, fpgaSVFContent, strlen(fpgaSVFContent));
    free(fpgaSVFContent);

    JSON* jsonExperimentConfig = JSONCreateObject();
    JSONAddFalseToObject(jsonExperimentConfig, "PS");
    JSONAddStringToObject(jsonExperimentConfig, "ExperimentType", jsonExperimentType->valuestring);

    JSONDeleteItemFromObject(deviceDataJSON, "ExperimentType");
    JSONAddItemToObject(deviceDataJSON, "Experiment", jsonExperimentConfig);

    deviceData = JSONPrint(deviceDataJSON);
    sendMessageWebsocket(wscLabserver.wsi, deviceData);

    pthread_join(wscLabserver.thread, NULL);
    pthread_join(commandService->thread, NULL);
    pthread_join(programmingService->thread, NULL);

    sigint_handler(0);

    return 0;
}