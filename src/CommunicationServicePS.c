#define EXPERIMENTDATA_FILENAME "ExperimentData.json"
#define FPGASVF_FILENAME "FPGA.svf"

#include "interfaces/ipcsockets.h"
#include "interfaces/websockets.h"
#include "utils/utils.h"
#include "parsers/json.h"
#include "logging/log.h"

//TODO add signal handler for updates

/* global variables needed for execution */
static websocketConnection wscLabserver;            // the websocket to the Labserver
static websocketConnection wscControlUnit;          // the websocket to the Control Unit
static IPCSocketConnection* protectionService;      // the IPC-socket to the Protection Service
static IPCSocketConnection* initializationService;  // the IPC-socket to the Initialization Service
static IPCSocketConnection* webcamService;          // the IPC-socket to the Webcam Service
static IPCSocketConnection* programmingService;     // the IPC-socket to the Programming Service
static unsigned int deviceID;                       // here we save the DeviceID
static JSON* deviceDataCompactJSON;                 // here a compact version of the DeviceData is kept in JSON-format
static char* deviceDataCompact;                     // here a compact version of the DeviceData is kept as a string
static unsigned int initializingPS = 0;             // indicates whether the physical system is currently being initialized

/* struct containing result of service initializations */
volatile struct 
{
    int protectionService;
    int initializationService;
    int webcamService;
} ServiceInitializations;

/*
 * the signal handler
 * sig - the signal the program received
 */
static void signal_handler(int sig)
{
    wscLabserver.interrupted = 1;
    pthread_join(wscLabserver.thread, NULL);
    wscControlUnit.interrupted = 1;
    pthread_join(wscControlUnit.thread, NULL);
    if(protectionService && protectionService->open)
        closeIPCConnection(protectionService);
    if(webcamService && webcamService->open)
        closeIPCConnection(webcamService);
    if(initializationService && initializationService->open)
        closeIPCConnection(initializationService);
    free(deviceDataCompact);
    if (sig == SIGINT)
    {
        exit(0);
    }
    else if (sig == SIGUSR1)
    {
        system("shutdown -r 1");
    }
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

        /* start sending data and send ExperimentInitAck */
        case WebsocketCommandExperimentInit:
        {
            log_debug("received experiment initialization message from labserver");
            char* experimentID = JSONGetObjectItem(JSONGetObjectItem(msgJSON, "data"), "ExperimentID")->valuestring;
            unsigned int virtualPartner = JSONIsTrue(JSONGetObjectItem(msgJSON, "virtualPartner"));
            JSON* experimentInitAck = JSONCreateObject();
            JSONAddNumberToObject(experimentInitAck, "SenderID", deviceID);
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
            JSONAddItemReferenceToObject(experimentInitAck, "Experiment", JSONGetObjectItem(deviceDataCompactJSON, "Experiment"));
            JSONAddNullToObject(experimentInitAck, "ActuatorData");
            char* msgString = JSONPrint(experimentInitAck);
            sendMessageIPC(protectionService, IPCMSGTYPE_EXPERIMENTINIT, msgString, strlen(msgString));
            free(msgString);
            JSONDelete(experimentInitAck);
            break;
        }

        /* stop sending data, stop all actuators and send ExperimentCloseAck */
        case WebsocketCommandExperimentClose:
        {
            log_debug("received experiment close message from labserver");
            //TODO add steps to stop current experiment execution
            //send message to protection service and maybe webcam service to stop their execution
            JSON* experimentCloseAckJSON = JSONCreateObject();
            JSONAddNumberToObject(experimentCloseAckJSON, "Command", WebsocketCommandExperimentCloseAck);
            char* experimentCloseAck = JSONPrint(experimentCloseAckJSON);

            sendMessageWebsocket(wsi, experimentCloseAck);
            //TODO maybe change to IPCMSGTYPE_ENDEXPERIMENT
            sendMessageIPC(protectionService, IPCMSGTYPE_STOPPHYSICALSYSTEM, NULL, 0);

            JSONDelete(experimentCloseAckJSON);
            free(experimentCloseAck);
            break;
        }

        /* start accepting actuator values from the control unit */
        case WebsocketCommandRunPS:
        {
            log_debug("received run physical system message from labserver");
            sendMessageIPC(protectionService, IPCMSGTYPE_RUNPHYSICALSYSTEM, NULL, 0);
            break;
        }

        /* stop accepting actuator values from the control unit */
        case WebsocketCommandStopPS:
        {
            log_debug("received stop physical system message from labserver");
            sendMessageIPC(protectionService, IPCMSGTYPE_STOPPHYSICALSYSTEM, NULL, 0);
            if (initializingPS)
            {
                initializingPS = 0;
                sendMessageIPC(initializationService, IPCMSGTYPE_STOPINITIALIZATION, NULL, 0);
            }
            break;
        }

        /* forward to Protection Service if not currently running an initialization program */
        case WebsocketCommandActuatorData:
        {
            log_debug("received actuator data message from control unit");
            if (!initializingPS)
            {
                JSON* actuatorDataJSON = JSONGetObjectItem(msgJSON, "ActuatorData");
                char* actuatorData = JSONPrint(actuatorDataJSON);
                sendMessageIPC(protectionService, IPCMSGTYPE_ACTUATORDATA, actuatorData, strlen(actuatorData));
                free(actuatorData);
            }
            break;
        }

        /* de-/activate the light */
        case WebsocketCommandLight:
        {
            log_debug("received light switch message from labserver");
            //TODO implement the light switching mechanism
            break;
        }

        /* used to set the values of virtual sensors e.g. User Button 3AxisPortal */
        case WebsocketCommandUserVariable:
        {
            log_debug("received user variable message from labserver");
            sendMessageIPC(protectionService, IPCMSGTYPE_SETUSERVARIABLE, message, strlen(message));
            break;
        }

        /* initializes the physical system to the chosen state */
        case WebsocketCommandInitPS:
        {
            log_debug("received physical system initialization message from labserver");
            unsigned char initializer = JSONGetObjectItem(msgJSON, "Initializer")->valueint;
            initializingPS = 1;
            sendMessageIPC(initializationService, IPCMSGTYPE_STARTINITIALIZATION, &initializer, 1);
            break;
        }
        
        /* control unit acknowledged the delay fault, calculate rtt and exit after some time if the delay fault doesn't get resolved */
        case WebsocketCommandDelayFaultAck:
        {
            log_debug("received delay fault ack message from control unit");
            sendMessageIPC(protectionService, IPCMSGTYPE_DELAYBASEDFAULTACK, message, strlen(message));
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
                        log_debug("received new actuator data from Initialization Service");
                        sendMessageIPC(protectionService, IPCMSGTYPE_ACTUATORDATA, msg.content, msg.length);
                        break;
                    }

                    case IPCMSGTYPE_SENSORDATA:
                    {
                        log_debug("received new sensor data from Protection Service");

                        if (initializingPS)
                        {
                            sendMessageIPC(initializationService, IPCMSGTYPE_SENSORDATA, msg.content, msg.length);
                        }

                        JSON* msgJSON = JSONParse(msg.content);

                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandSensorData);

                        char* message = JSONPrint(msgJSON);
                        if (wscControlUnit.connectionEstablished && !wscControlUnit.interrupted)
                        {
                            sendMessageWebsocket(wscControlUnit.wsi, message);
                        }
                        else
                        {
                            sendMessageWebsocket(wscLabserver.wsi, message);
                        }

                        free(message);
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_DELAYBASEDFAULT:
                    {
                        log_debug("received delay based fault message from Protection Service");
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandDelayFault);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        if (wscControlUnit.connectionEstablished && !wscControlUnit.interrupted)
                        {
                            sendMessageWebsocket(wscControlUnit.wsi, message);
                        }
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_DELAYBASEDERROR:
                    {
                        log_debug("received delay based error message from Protection Service");
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandDelayError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_USERBASEDERROR:
                    {
                        log_debug("received user based error message from Protection Service");
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandUserError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_INFRASTRUCTUREBASEDERROR:
                    {
                        log_debug("received infrastructure based error message from Protection Service");
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", deviceID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandInfrastructureError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_INITPROTECTIONFINISHED:
                    {
                        log_debug("received initialization finished message from Protection Service");
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            log_debug("initialization of Protection Service failed");
                            ServiceInitializations.protectionService = -1;
                        }
                        else
                        {
                            log_debug("initialization of Protection Service succeded");
                            ServiceInitializations.protectionService = 1;
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED:
                    {
                        log_debug("received initialization finished message from Initialization Service");
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            log_debug("initialization of Initialization Service failed");
                            ServiceInitializations.initializationService = -1;
                        }
                        else
                        {
                            log_debug("initialization of Initialization Service succeded");
                            ServiceInitializations.initializationService = 1;
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITIALIZATIONFINISHED:
                    {
                        log_debug("received initialization of physical system finished message from Initialization Service");
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            log_debug("initialization of Physical System failed");
                            //TODO maybe add something but doesn't seem necessary right now
                        }
                        else
                        {   
                            log_debug("initialization of Physical System succeded");
                            //TODO maybe add something but doesn't seem necessary right now
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITWEBCAMSERVICEFINISHED:
                    {
                        log_debug("received initialization finished message from Webcam Service");
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            ServiceInitializations.webcamService = -1;
                        }
                        else
                        {
                            ServiceInitializations.webcamService = 1;
                        }
                        break;
                    }

                    case IPCMSGTYPE_EXPERIMENTINIT:
                    {
                        log_debug("received experiment initialization message from Protection Service");
                        sendMessageWebsocket(wscLabserver.wsi, msg.content);
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
    signal(SIGINT, signal_handler);
    signal(SIGUSR1, signal_handler);

    /* create all needed sockets (except serversocket) */
    if(websocketPrepareContext(&wscLabserver, WEBSOCKET_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wscLabserver.connectionEstablished);

    ServiceInitializations.protectionService = 0;
    protectionService = connectToIPCSocket(PROTECTION_SERVICE, messageHandlerIPC);
    if (protectionService == NULL)
    {
        return -1;
    }

    ServiceInitializations.initializationService = 0;
    initializationService = connectToIPCSocket(INITIALIZATION_SERVICE, messageHandlerIPC);
    if (initializationService == NULL)
    {
        return -1;
    }

    ServiceInitializations.webcamService = 0;
    webcamService = connectToIPCSocket(WEBCAM_SERVICE, messageHandlerIPC);
    if (webcamService == NULL)
    {
        return -1;
    }

    programmingService = connectToIPCSocket(PROGRAMMING_SERVICE, messageHandlerIPC);
    if (programmingService == NULL)
    {
        return -1;
    }

    /* read experiment configuration file and initialize all services */
    //TODO change paths, add NULL and error handling
    char* deviceConfigContent = readFile("/data/GOLDiServices/DeviceData.json", NULL);
    JSON* jsonDeviceConfig = JSONParse(deviceConfigContent);
    JSON* jsonDeviceID = JSONGetObjectItem(jsonDeviceConfig, "DeviceID");

    /* find experiment config file and read content */
    char* experimentType = JSONGetObjectItem(jsonDeviceConfig, "ExperimentType")->valuestring;
    char* experimentsPath = "/etc/GOLDiServices/experiments/";
    char* completeConfigPath = malloc(strlen(experimentType) + strlen(experimentsPath) + strlen(EXPERIMENTDATA_FILENAME) + 2);
    strcpy(completeConfigPath, experimentsPath);
    strcat(completeConfigPath, experimentType);
    strcat(completeConfigPath, "/");
    strcat(completeConfigPath, EXPERIMENTDATA_FILENAME);
    char* experimentConfigContent = readFile(completeConfigPath, NULL);
    free(completeConfigPath);

    /* find fpga programming file and read content */ 
    char* fpgaSVFPath = malloc(strlen(experimentType) + strlen(experimentsPath) + strlen(FPGASVF_FILENAME) + 2);
    strcpy(fpgaSVFPath, experimentsPath);
    strcat(fpgaSVFPath, experimentType);
    strcat(fpgaSVFPath, "/");
    strcat(fpgaSVFPath, FPGASVF_FILENAME);

    JSON* jsonExperimentConfig = JSONParse(experimentConfigContent);
    free(experimentConfigContent);
    JSON* jsonSensors = JSONGetObjectItem(jsonExperimentConfig, "Sensors");
    JSON* jsonActuators = JSONGetObjectItem(jsonExperimentConfig, "Actuators");
    JSON* jsonProtection = JSONGetObjectItem(jsonExperimentConfig, "ProtectionRules");
    JSON* jsonInitializers = JSONGetObjectItem(jsonExperimentConfig, "Initializers");

    /* variables needed for the initialization of the Webcam Service*/
    JSON* jsonCamera = JSONGetObjectItem(jsonDeviceConfig, "Camera");
    JSON* jsonCameraType = JSONGetObjectItem(jsonCamera, "Type");
    JSON* jsonCameraAddress = JSONGetObjectItem(jsonCamera, "Address");

    /* before continuing to parse try to create serversocket for wsc with Control Unit */
    char* ipAdress = JSONGetObjectItem(JSONGetObjectItem(jsonDeviceConfig, "Network"), "LocalIP")->valuestring;
    if(websocketPrepareContext(&wscControlUnit, WEBSOCKET_PROTOCOL, ipAdress, GOLDi_SERVERPORT, handleWebsocketMessage, 1))
    {
        return -1;
    }

    /* program the fpga */
    sendMessageIPC(programmingService, IPCMSGTYPE_PROGRAMFPGA, fpgaSVFPath, strlen(fpgaSVFPath));
    free(fpgaSVFPath);

    /* initialize Protection Service */
    log_info("initializing Protection Service");
    JSON* jsonProtectionInitMsg = JSONCreateObject();
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "Sensors", jsonSensors);
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "Actuators", jsonActuators);
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "ProtectionRules", jsonProtection);
    char* stringProtectionInitMsg = JSONPrint(jsonProtectionInitMsg);
    sendMessageIPC(protectionService, IPCMSGTYPE_INITPROTECTIONSERVICE, stringProtectionInitMsg, strlen(stringProtectionInitMsg));

    JSONDelete(jsonProtectionInitMsg);
    free(stringProtectionInitMsg);

    log_info("waiting for Protection Service to finish initializing");
    while(!(ServiceInitializations.protectionService > 0))
    {
        if (ServiceInitializations.protectionService == -1)
        {
            //TODO error handling
            log_error("the Protection Service initialization failed");
            return -1;
        }
    }
    log_info("the Protection Service was initialized successfully");

    /* initialize Initialization Service */
    log_info("initializing Initialization Service");
    JSON* jsonInitializationInitMsg = JSONCreateObject();
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Sensors", jsonSensors);
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Actuators", jsonActuators);
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Initializers", jsonInitializers);
    char* stringInitializationInitMsg = JSONPrint(jsonInitializationInitMsg);
    sendMessageIPC(initializationService, IPCMSGTYPE_INITINITIALIZATION, stringInitializationInitMsg, strlen(stringInitializationInitMsg));

    JSONDelete(jsonInitializationInitMsg);
    free(stringInitializationInitMsg);

    log_info("waiting for Initialization Service to finish initializing");
    while(!(ServiceInitializations.initializationService > 0))
    {
        if (ServiceInitializations.initializationService == -1)
        {
            //TODO error handling
            log_error("the Initialization Service initialization failed");
            return -1;
        }
    }
    log_info("the Initialization Service was initialized successfully");

    /* initialize Webcam Service */
    log_info("initializing Webcam Service");
    JSON* jsonWebcamInitMsg = JSONCreateObject();
    JSONAddItemReferenceToObject(jsonWebcamInitMsg, "Type", jsonCameraType);
    JSONAddItemReferenceToObject(jsonWebcamInitMsg, "Address", jsonCameraAddress);
    JSONAddItemReferenceToObject(jsonWebcamInitMsg, "ID", jsonDeviceID);
    char* stringWebcamInitMsg = JSONPrint(jsonWebcamInitMsg);
    sendMessageIPC(webcamService, IPCMSGTYPE_INITWEBCAMSERVICE, stringWebcamInitMsg, strlen(stringWebcamInitMsg));

    JSONDelete(jsonWebcamInitMsg);
    free(stringWebcamInitMsg);

    log_info("waiting for Webcam Service to finish initializing");
    while(!(ServiceInitializations.webcamService > 0))
    {
        if (ServiceInitializations.webcamService == -1)
        {
            //TODO error handling
            log_error("the Webcam Service initialization failed");
            return -1;
        }
    }
    log_info("the Webcam Service was initialized successfully");

    /* prepare experiment data for Labserver and Control Unit */
    JSON* jsonSensor = NULL;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        JSONDeleteItemFromObject(jsonSensor, "SensorCommand");
        JSONDeleteItemFromObject(jsonSensor, "SensorMathExpression");
    }

    JSON* jsonActuator = NULL;
    JSONArrayForEach(jsonActuator, jsonActuators)
    {
        JSONDeleteItemFromObject(jsonActuator, "ActuatorCommand");
        JSONDeleteItemFromObject(jsonActuator, "ActuatorMathExpression");
    }

    JSON* jsonInitializer = NULL;
    char** initializerStrings = malloc(sizeof(*initializerStrings) * JSONGetArraySize(jsonInitializers));
    unsigned int initializerIndex = 0;
    JSONArrayForEach(jsonInitializer, jsonInitializers)
    {
        initializerStrings[initializerIndex] = jsonInitializer->string;
        initializerIndex++;
    }
    JSON* jsonInitializersNew = JSONCreateStringArray((const char **)initializerStrings, initializerIndex);
    JSONDeleteItemFromObject(jsonExperimentConfig, "Initializer");
    JSONAddItemToObject(jsonExperimentConfig, "Initializer", jsonInitializersNew);

    JSONDeleteItemFromObject(jsonDeviceConfig, "ExperimentType");
    JSONAddTrueToObject(jsonExperimentConfig, "PS");
    JSONAddItemToObject(jsonDeviceConfig, "Experiment", jsonExperimentConfig);

    /* save compact device data and send it to the Labserver */
    deviceDataCompact = JSONPrint(jsonDeviceConfig);
    deviceDataCompactJSON = JSONParse(deviceDataCompact);
    deviceID = jsonDeviceID->valueint;
    JSONAddNumberToObject(jsonDeviceConfig, "Command", WebsocketCommandDeviceData);
    char* deviceDataCommand = JSONPrint(jsonDeviceConfig);
    sendMessageWebsocket(wscLabserver.wsi, deviceDataCommand);

    /* cleanup */
    JSONDelete(jsonDeviceConfig);
    free(deviceConfigContent);
    free(deviceDataCommand);

    pthread_join(wscLabserver.thread, NULL);
    pthread_join(protectionService->thread, NULL);
    pthread_join(webcamService->thread, NULL);
    pthread_join(initializationService->thread, NULL);

    signal_handler(SIGINT);

    return 0;
}