#include "interfaces/ipcsockets.h"
#include "interfaces/websockets.h"
#include "utils/utils.h"
#include "parsers/json.h"

// TODO maybe add explanation for variables
/* global variables needed for execution */
static websocketConnection wscLabserver;
static websocketConnection wscControlUnit;
static IPCSocketConnection* protectionService;
static IPCSocketConnection* initializationService;
static IPCSocketConnection* webcamService;
static unsigned int deviceID;
static JSON* deviceDataCompactJSON;
static char* deviceDataCompact;
static unsigned int initializingPS = 0;
static unsigned int SenderID;
static unsigned int forwardSensorData = 0;

static void sigint_handler(int sig)
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
            break;
        }

        /* start sending data and send ExperimentInitAck */
        case WebsocketCommandExperimentInit:
        {
            //TODO if partner is real then send experiment data with current sensor values
            //TODONOTE add ActuatorData with value null to experiment data
            //look at CommunicationServiceCU to see how experimentInitAck is handled there
            int experimentID = JSONGetObjectItem(JSONGetObjectItem(msgJSON, "data"), "ExperimentID");
            unsigned int virtualPartner = JSONIsTrue(JSONGetObjectItem(msgJSON, "virtualPartner"));
            JSON* experimentInitAck = JSONCreateObject();
            JSONAddNumberToObject(experimentInitAck, "SenderID", SenderID);
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
            char* msgString = JSONPrint(experimentInitAck);
            sendMessageWebsocket(wsi, msgString);
            free(msgString);
            JSONDelete(experimentInitAck);
            break;
        }

        /* stop sending data, stop all actuators and send ExperimentCloseAck */
        case WebsocketCommandExperimentClose:
        {
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
            sendMessageIPC(protectionService, IPCMSGTYPE_RUNPHYSICALSYSTEM, NULL, 0);
            break;
        }

        /* stop accepting actuator values from the control unit */
        case WebsocketCommandStopPS:
        {
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
            //TODO implement the light switching mechanism
            break;
        }

        /* used to set the values of virtual sensors e.g. User Button 3AxisPortal */
        case WebsocketCommandUserVariable:
        {
            sendMessageIPC(protectionService, IPCMSGTYPE_SETUSERVARIABLE, message, strlen(message));
            break;
        }

        /* initializes the physical system to the chosen state */
        case WebsocketCommandInitPS:
        {
            unsigned char initializer = JSONGetObjectItem(msgJSON, "Initializer")->valueint;
            initializingPS = 1;
            sendMessageIPC(initializationService, IPCMSGTYPE_STARTINITIALIZATION, &initializer, 1);
            break;
        }
        
        /* control unit acknowledged the delay fault, calculate rtt and exit after some time if the delay fault doesn't get resolved */
        case WebsocketCommandDelayFaultAck:
        {
            sendMessageIPC(protectionService, IPCMSGTYPE_DELAYBASEDFAULTACK, message, strlen(message));
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
                printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_DELAYBASEDFAULT:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", SenderID);
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
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", SenderID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandDelayError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_USERBASEDERROR:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", SenderID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandUserError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_INFRASTRUCTUREBASEDERROR:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSONAddNumberToObject(msgJSON, "SenderID", SenderID);
                        JSONAddNumberToObject(msgJSON, "Command", WebsocketCommandInfrastructureError);
                        char* message = JSONPrint(msgJSON);
                        sendMessageWebsocket(wscLabserver.wsi, message);
                        JSONDelete(msgJSON);
                        free(message);
                        break;
                    }

                    case IPCMSGTYPE_INITPROTECTIONFINISHED:
                    {
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            //TODO add graceful shutdown or retry
                        }
                        else
                        {
                            //TODO maybe add initfinished variable
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED:
                    {
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            //TODO add graceful shutdown or retry
                        }
                        else
                        {
                            //TODO maybe add initfinished variable
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITIALIZATIONFINISHED:
                    {
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            //TODO maybe add something but doesn't seem necessary right now
                        }
                        else
                        {   
                            //TODO maybe add something but doesn't seem necessary right now
                        }
                        break;
                    }

                    case IPCMSGTYPE_INITPROGRAMMINGSERVICEFINISHED:
                    {
                        int success = deserializeInt(msg.content);
                        if (!success)
                        {
                            //TODO add graceful shutdown or retry
                        }
                        else
                        {
                            //TODO maybe add initfinished variable
                        }
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

    /* create all needed sockets (except serversocket) */
    if(websocketPrepareContext(&wscLabserver, COMMUNICATION_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wscLabserver.connectionEstablished);

    protectionService = connectToIPCSocket(PROTECTION_SERVICE, messageHandlerIPC);
    if (protectionService == NULL)
        return -1;

    initializationService = connectToIPCSocket(INITIALIZATION_SERVICE, messageHandlerIPC);
    if (initializationService == NULL)
        return -1;

    webcamService = connectToIPCSocket(WEBCAM_SERVICE, messageHandlerIPC);
    if (webcamService == NULL)
        return -1;

    /* read experiment configuration file and initialize all services */
    //char* deviceConfigContent = readFile("/data/GOLDiServices/DeviceData.json");
    char* deviceConfigContent = readFile("../../../Testfiles/ExperimentConfig.json");

    JSON* jsonDeviceConfig = JSONParse(deviceConfigContent);
    JSON* jsonExperimentConfig = JSONGetObjectItem(jsonDeviceConfig, "Experiment");
    JSON* jsonSensors = JSONGetObjectItem(jsonExperimentConfig, "Sensors");
    JSON* jsonActuators = JSONGetObjectItem(jsonExperimentConfig, "Actuators");
    JSON* jsonProtection = JSONGetObjectItem(jsonExperimentConfig, "ProtectionRules");
    JSON* jsonInitializers = JSONGetObjectItem(jsonExperimentConfig, "Initializers");

    /**
     * char* experimentName = JSONGetObjectItem(jsonExperimentConfig, "ExperimentType");
     * char* experimentsPath = "/etc/GOLDiServices/experiments/"
     * char* conmpleteConfigPath = malloc(strlen(experimentName) + strlen(experimentsPath) + 1);
     * strcat(completeConfigPath, experimentName, experimentsPath);
     * char* experimentConfigContent = readFile(completeConfigPath);
     * */

    /* before continuing to parse try to create serversocket for wsc with Control Unit */
    char* ipAdress = JSONGetObjectItem(JSONGetObjectItem(jsonDeviceConfig, "Network"), "LocalIP")->valuestring;
    if(websocketPrepareContext(&wscControlUnit, COMMUNICATION_PROTOCOL, ipAdress, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    JSON* jsonProtectionInitMsg = JSONCreateObject();
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "Sensors", jsonSensors);
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "Actuators", jsonActuators);
    JSONAddItemReferenceToObject(jsonProtectionInitMsg, "ProtectionRules", jsonProtection);
    char* stringProtectionInitMsg = JSONPrint(jsonProtectionInitMsg);
    sendMessageIPC(protectionService, IPCMSGTYPE_INITPROTECTIONSERVICE, stringProtectionInitMsg, strlen(stringProtectionInitMsg));

    JSONDelete(jsonProtectionInitMsg);
    free(stringProtectionInitMsg);

    JSON* jsonInitializationInitMsg = JSONCreateObject();
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Sensors", jsonSensors);
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Actuators", jsonActuators);
    JSONAddItemReferenceToObject(jsonInitializationInitMsg, "Initializers", jsonInitializers);
    char* stringInitializationInitMsg = JSONPrint(jsonInitializationInitMsg);
    sendMessageIPC(protectionService, IPCMSGTYPE_INITINITIALIZATION, stringInitializationInitMsg, strlen(stringInitializationInitMsg));

    JSONDelete(jsonInitializationInitMsg);
    free(stringInitializationInitMsg);

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

    JSON* jsonInitializersNew = JSONCreateStringArray(initializerStrings, initializerIndex);
    JSONDeleteItemFromObject(jsonExperimentConfig, "Initializer");
    JSONAddItemToObject(jsonExperimentConfig, "Initializer", jsonInitializersNew);

    deviceDataCompact = JSONPrint(jsonDeviceConfig);
    deviceDataCompactJSON = JSONParse(deviceDataCompact);
    SenderID = JSONGetObjectItem(deviceDataCompactJSON, "DeviceID")->valueint;
    JSONAddNumberToObject(jsonDeviceConfig, "Command", WebsocketCommandDeviceData);
    char* deviceDataCommand = JSONPrint(jsonDeviceConfig);
    sendMessageWebsocket(wscLabserver.wsi, deviceDataCommand);

    JSONDelete(jsonDeviceConfig);
    free(deviceConfigContent);
    free(deviceDataCommand);

    pthread_join(wscLabserver.thread, NULL);
    pthread_join(protectionService->thread, NULL);
    pthread_join(webcamService->thread, NULL);
    pthread_join(initializationService->thread, NULL);

    sigint_handler(0);

    return 0;
}