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
    log_debug("entered websocket message handler");
    int result = 0;
    JSON* msgJSON = JSONParse(message);
    if (msgJSON != NULL)
    {
        log_debug("parsed websocket message to json successfully");
    }
    else 
    {
        log_debug("parsing websocket message to json failed");
        log_debug("messageLength: %d", strlen(message));
        log_debug("message: %s", message);
    }
    JSON* msgCommand = JSONGetObjectItem(msgJSON, "Command");
    if (msgCommand != NULL)
    {
        log_debug("parsed command from websocket message json successfully");
    }
    else 
    {
        log_debug("parsing command from websocket message json failed");
    }

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
            log_debug("received experiment data message from labserver");
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
            log_debug("received experiment initialization message from labserver");
            /* initializing experiment init ack for later use (when command service initialization finished) */
            JSON* experimentDataJSON = JSONGetObjectItem(msgJSON, "data");
            JSON* experimentIDJSON = JSONGetObjectItem(experimentDataJSON, "ExperimentID");
            char* experimentID = experimentIDJSON->valuestring;
            unsigned int virtualPartner = JSONIsTrue(JSONGetObjectItem(msgJSON, "virtualPartner"));
            experimentInitAck = JSONCreateObject();
            JSONAddNumberToObject(experimentInitAck, "SenderID", JSONGetObjectItem(deviceDataJSON, "DeviceID")->valueint);
            JSONAddNumberToObject(experimentInitAck, "Command", WebsocketCommandExperimentInitAck);
            JSONAddStringToObject(experimentInitAck, "ExperimentID", experimentID);
            if (virtualPartner == 0)
            {
                JSONAddFalseToObject(experimentInitAck, "virtualPartner");
            }
            else
            {
                JSONAddTrueToObject(experimentInitAck, "virtualPartner");
            }
            if (experimentInitAck == NULL)
            {
                log_error("error with experiment init ack json");
            }
            else
            {
                char* initackstring = JSONPrint(experimentInitAck);
                log_debug("experiment init ack: %s", initackstring);
                free(initackstring);
            }
            
            break;
        }

        /* close connection to physical system if created, stop sending data and send ExperimentCloseAck */
        case WebsocketCommandExperimentClose:
        {
            log_debug("received experiment close message from labserver");
            wscPhysicalSystem.interrupted = 1;
            if (wscPhysicalSystem.connectionEstablished)
            {
                pthread_join(wscPhysicalSystem.thread, NULL);
            }
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
            log_debug("received delay fault message from physical system");
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
            log_debug("received reset control unit message from labserver");
            //TODO add implementation
            break;
        }

        /* forward to Command Service */
        case WebsocketCommandSensorData:
        {
            log_debug("received sensor data message from physical system");
            JSON* sensorDataJSON = JSONGetObjectItem(msgJSON, "SensorData");
            char* sensorData = JSONPrint(sensorDataJSON);
            sendMessageIPC(commandService, IPCMSGTYPE_SENSORDATA, sensorData, strlen(sensorData));
            free(sensorData);
            break;
        }
        
        /* used to send the programming file as a base64 encoded string, decode, send to Programming Service and send ack*/
        case WebsocketCommandProgramCU:
        {
            //TODO
            log_debug("received program control unit message from labserver");
            unsigned int length = 0;
            char* programData = decodeBase64(JSONGetObjectItem(msgJSON, "File")->valuestring, &length);
            log_debug("value of programming file: %s", JSONGetObjectItem(msgJSON, "File")->valuestring);
            log_debug("length of decoded string: %d", length);
            FILE *write_ptr = fopen("/tmp/GOLDiServices/ProgrammingService/programmingfile","wb");
            fwrite(programData, 1, length, write_ptr);
            fclose(write_ptr);
            sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0);
            sendMessageIPC(commandService, IPCMSGTYPE_PROGRAMCONTROLUNIT, NULL, 0);
            break;
        }

        /* try to connect to Physical System and send ack with outcome accordingly */
        case WebsocketCommandDirectConnectionInit:
        {
            log_debug("received direct connection initialization message from labserver");
            JSON* directConnectionAckJSON = JSONCreateObject();
            JSONAddNumberToObject(directConnectionAckJSON, "Command", WebsocketCommandDirectConnectionAck);
            char* subnetPS = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "Subnet")->valuestring;
            char* ipAdressPS = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "IP")->valuestring;
            char* subnetCU = JSONGetObjectItem(JSONGetObjectItem(deviceDataJSON, "Network"), "Subnet")->valuestring;
            if (!strcmp(subnetPS, subnetCU)) 
            {
                if(websocketPrepareContext(&wscPhysicalSystem, WEBSOCKET_PROTOCOL, ipAdressPS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
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
            log_error("received websocket message of unknown type");
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
                        log_debug("received actuator data message from Command Service");
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
                        log_debug("received initialization finished message from Command Service");
                        if (msg.length == 0)
                        {
                            // an error has occured TODO error handling
                            log_error("the Command Service could not be initialized correctly");
                        }
                        else
                        {
                            log_debug("the Command Service was initialized correctly");
                            JSON* msgJSON = JSONParse(msg.content);
                            if (msgJSON == NULL)
                            {
                                log_error("could not parse initialization response from Command Service to json");
                            }
                            JSON* actuatorDataJSON = JSONGetObjectItem(msgJSON, "ActuatorData");
                            if (actuatorDataJSON == NULL)
                            {
                                log_error("could not grab actuatordata from initialization response json of Command Service");
                            }
                            if (experimentInitAck == NULL)
                            {
                                log_error("error with experiment init ack json");
                            }
                            JSONAddNullToObject(experimentInitAck, "Experiment");
                            JSONAddNullToObject(experimentInitAck, "SensorData");
                            JSONAddItemReferenceToObject(experimentInitAck, "ActuatorData", actuatorDataJSON);
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
                        log_debug("received initialization finished message from Programming Service");
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
                        log_debug("received delay fault ack message from Command Service");
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
                        log_debug("received programming control unit finished message from Programming Service");
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
                        log_error("received IPC message of unknown type");
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
    if(websocketPrepareContext(&wscLabserver, WEBSOCKET_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
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

    while (initializedProgrammingService <= 0)
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
    strcpy(fpgaSVFPath, "/etc/GOLDiServices/controlunits/");
    strcat(fpgaSVFPath, experimentType);
    strcat(fpgaSVFPath, "/");
    strcat(fpgaSVFPath, FPGASVF_FILENAME);

    /* program the fpga */
    sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMFPGA, fpgaSVFPath, strlen(fpgaSVFPath));
    free(fpgaSVFPath);

    JSON* jsonExperimentConfig = JSONCreateObject();
    JSONAddFalseToObject(jsonExperimentConfig, "PS");
    JSONAddStringToObject(jsonExperimentConfig, "ExperimentType", jsonExperimentType->valuestring);

    JSONDeleteItemFromObject(deviceDataJSON, "ExperimentType");
    JSONAddItemToObject(deviceDataJSON, "Experiment", jsonExperimentConfig);
    JSONAddNumberToObject(deviceDataJSON, "Command", WebsocketCommandDeviceData);

    deviceData = JSONPrint(deviceDataJSON);
    sendMessageWebsocket(wscLabserver.wsi, deviceData);

    pthread_join(wscLabserver.thread, NULL);
    pthread_join(commandService->thread, NULL);
    pthread_join(programmingService->thread, NULL);

    sigint_handler(0);

    return 0;
}