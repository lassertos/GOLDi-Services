#define EXTENDED_SENSORS_ACTUATORS

#include "parsers/StateMachine.h"
#include "interfaces/SensorsActuators.h"
#include "utils/utils.h"
#include "interfaces/ipcsockets.h"
#include "logging/log.h"

static IPCSocketConnection* communicationService;
Sensor* sensors;
Actuator* actuators;
Variable* variables;
StateMachine* stateMachines;
unsigned int sensorCount;
unsigned int actuatorCount;
unsigned int stateMachineCount;
StateMachineExecution execution = {NULL, 0};

ActuatorDataPacket StateMachineOutputToActuatorDataPacket(StateMachineOutput output)
{
    ActuatorDataPacket packet = {output.actuatorID, output.type, output.value};
    return packet;
}

int startInitialization(void)
{
    pthread_t executionThread;
    int result = 0;
    char* currentState = execution.stateMachine->activeState->name;
    pthread_create(&executionThread, NULL, &executeStateMachine, &execution);
    while(!execution.stopped)
    {
        if (strcmp(currentState, execution.stateMachine->activeState->name) != 0)
        {
            StateMachineOutput* outputs = execution.stateMachine->activeState->outputs;
            unsigned int outputsCount = execution.stateMachine->activeState->outputCount;
            JSON* actuatorDataMsgJSON = JSONCreateObject();
            JSON* actuatorDataJSON = JSONCreateArray();
            for (int i = 0; i < actuatorCount; i++)
            {
                ActuatorDataPacket packet = {actuators[i].actuatorID, actuators[i].type, actuators[i].stopValue};
                JSON* packetJSON = ActuatorDataPacketToJSON(packet);
                JSONAddItemToArray(actuatorDataJSON, packetJSON);
            }
            for (int i = 0; i < outputsCount; i++)
            {
                ActuatorDataPacket packet = StateMachineOutputToActuatorDataPacket(outputs[i]);
                JSON* packetJSON = ActuatorDataPacketToJSON(packet);
                JSONReplaceItemInObject(actuatorDataJSON, packetJSON->string, packetJSON);
            }
            JSONAddItemToObject(actuatorDataMsgJSON, "ActuatorData", actuatorDataJSON);
            char* actuatorDataMsg = JSONPrint(actuatorDataMsgJSON);
            sendMessageIPC(communicationService, IPCMSGTYPE_ACTUATORDATA, actuatorDataMsg, strlen(actuatorDataMsg));
            JSONDelete(actuatorDataMsgJSON);
            free(actuatorDataMsg);
            currentState = execution.stateMachine->activeState->name;
        }
    }
    pthread_join(executionThread, &result);
    char* resultString = serializeInt(result);
    sendMessageIPC(communicationService, IPCMSGTYPE_INITIALIZATIONFINISHED, resultString, 4);
    free(resultString);
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
                    case IPCMSGTYPE_INITINITIALIZATION:
                    {
                        log_debug("initialization: starting initialization");
                        JSON* msgJSON = JSONParse(msg.content);
                        if (msgJSON == NULL)
                        {
                            log_error("initialization: IPC message could not be parsed to JSON");
                            break;
                        }
                        log_debug("initialization: parsing sensors as json from message json");
                        JSON* sensorsJSON = JSONGetObjectItem(msgJSON, "Sensors");
                        if (sensorsJSON == NULL)
                        {
                            log_error("initialization: sensors not included in message json");
                            break;
                        }
                        log_debug("initialization: parsing actuators as json from message json");
                        JSON* actuatorsJSON = JSONGetObjectItem(msgJSON, "Actuators");
                        if (actuatorsJSON == NULL)
                        {
                            log_error("initialization: actuators not included in message json");
                            break;
                        }
                        log_debug("initialization: parsing initializers as json from message json");
                        JSON* initializersJSON = JSONGetObjectItem(msgJSON, "Initializers");
                        if (initializersJSON == NULL)
                        {
                            log_error("initialization: initializers not included in message json");
                            break;
                        }
                        log_debug("initialization: converting sensors json to string");
                        char* stringSensors = JSONPrint(sensorsJSON);
                        if (stringSensors == NULL)
                        {
                            log_error("initialization: sensors json could not be parsed to string");
                            break;
                        }
                        log_debug("initialization: converting actuators json to string");
                        char* stringActuators = JSONPrint(actuatorsJSON);
                        if (stringActuators == NULL)
                        {
                            log_error("initialization: actuators json could not be parsed to string");
                            break;
                        }
                        log_debug("initialization: converting initializers json to string");
                        char* stringInitializers = JSONPrint(initializersJSON);
                        if (stringInitializers == NULL)
                        {
                            log_error("initialization: initializers json could not be parsed to string");
                            break;
                        }

                        log_debug("initialization: parsing sensors");
                        sensors = parseSensors(stringSensors, strlen(stringSensors), &sensorCount);
                        if (sensors == NULL)
                        {
                            log_error("initialization: sensors could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(ipcsc, IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < sensorCount; i++)
                            {
                                printSensorData(sensors[i]);   // TODO add debugging flag
                            }
                        }

                        log_debug("initialization: parsing actuators");
                        actuators = parseActuators(stringActuators, strlen(stringActuators), &actuatorCount, sensorCount);
                        if (actuators == NULL)
                        {
                            log_error("initialization: actuators could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(ipcsc, IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            variables = malloc(sizeof(*variables) * (sensorCount+actuatorCount));
                            for (int i = 0; i < sensorCount; i++)
                            {
                                OperandType operandType;
                                if (sensors[i].type == SensorTypeBinary)
                                {
                                    operandType = OperandTypeBinary;
                                }
                                else
                                {
                                    operandType = OperandTypeNumber;
                                }
                                variables[i] = (Variable){operandType, sensors[i].sensorID, sensors[i].value, getValueSizeOfSensorType(sensors[i].type)};
                            }
                            for (int i = sensorCount; i < (sensorCount + actuatorCount); i++)
                            {
                                int k = i-sensorCount;
                                variables[k] = (Variable){actuators[k].type, actuators[k].actuatorID, actuators[k].value, getValueSizeOfActuatorType(actuators[k].type)};
                                printActuatorData(actuators[k]);   // TODO add debugging flag
                            }
                        }

                        log_debug("initialization: parsing state machines of initializers");
                        stateMachines = parseStateMachines(stringInitializers, strlen(stringInitializers), variables, sensorCount+actuatorCount, &stateMachineCount);
                        if (stateMachines == NULL)
                        {
                            log_error("initialization: state machines of initializers could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(ipcsc, IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < stateMachineCount; i++)
                            {
                                printStateMachineInfo(&stateMachines[i]);   // TODO add debugging flag
                            }
                        }

                        log_debug("initialization: sending result to Communication Service");
                        char* result = serializeInt(1);
                        sendMessageIPC(ipcsc, IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED, result, 4);
                        free(result);

                        JSONDelete(msgJSON);
                        free(stringSensors);
                        free(stringActuators);
                        free(stringInitializers);
                        break;
                    }

                    case IPCMSGTYPE_SENSORDATA:
                    {
                        log_debug("receiving new sensor data");
                        unsigned int newSensorDataCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(msg.content, msg.length, &newSensorDataCount);
                        if (sensorDataPackets == NULL)
                        {
                            //TODO error handling
                        }
                        else
                        {
                            for (int i = 0; i < newSensorDataCount; i++)
                            {
                                Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                                sensor->value = sensorDataPackets[i].value;
                                free(sensorDataPackets[i].sensorID);
                            }
                        }
                        free(sensorDataPackets);
                        break;
                    }

                    case IPCMSGTYPE_STARTINITIALIZATION:
                    {
                        log_debug("starting initialization of physical system");
                        execution.stopped = 0;
                        execution.stateMachine = &stateMachines[msg.content[0]];
                        pthread_t initializationThread;
                        pthread_create(&initializationThread, NULL, &startInitialization, NULL);
                        break;
                    }

                    case IPCMSGTYPE_STOPINITIALIZATION:
                    {
                        log_debug("stopping initialization of physical system");
                        execution.stopped = 1;
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
                        log_error("received message of unknown type");
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
    return 0;
}

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(INITIALIZATION_SERVICE);
    communicationService = acceptIPCConnection(fd, messageHandlerIPC);
    if (communicationService == NULL)
    {
        log_error("connection to Communication Service could not be established!\n");
        return -1;
    }
    
    pthread_join(communicationService->thread, NULL);

    destroySensors(sensors, sensorCount);
    destroyActuators(actuators, actuatorCount);
    destroyStateMachines(stateMachines, stateMachineCount);
    free(variables);
    return 0;
}