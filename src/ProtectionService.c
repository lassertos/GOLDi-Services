#define SENSOR_PREFIX 'x'
#define ACTUATOR_PREFIX 'y'
#define EXTENDED_SENSORS_ACTUATORS

#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"
#include "utils/utils.h"

typedef enum
{
    DELAY_ERROR,
    USER_ERROR,
    INFRASTRUCTURE_ERROR
} ErrorType;

typedef struct
{
    BooleanExpression*  expression;
    ErrorType           errorType;
    char*               errorMessage;
    int                 errorCode;
} Protectionrule;

struct 
{
    Protectionrule* rules;
    int             count;
} Protectionrules;

typedef struct
{
    Protectionrule* rule;
    pthread_t       thread;
    unsigned int    isActive:1;
} DelayBasedFault;

struct 
{
    DelayBasedFault*    faults;
    unsigned int        maxAmount;
} delayBasedFaults;

static IPCSocketConnection* communicationService;
static Sensor* sensors;
static Actuator* actuators;
static Actuator* incomingActuators;
static unsigned int sensorCount;
static unsigned int actuatorCount;
static unsigned int stoppedPS = 1;
pthread_mutex_t mutexSPI;

void printProtectionRule(Protectionrule rule)
{
    printf("Expression: ");
    printBooleanExpression(rule.expression);
    printf("ErrorMessage: %s\n", rule.errorMessage);
    printf("ErrorCode: %d\n", rule.errorCode);
}

int parseProtectionRules(char *protectionString)
{
    JSON* protectionJSON = JSONParse(protectionString);
    if (protectionJSON == NULL)
    {
        return 1;
    }

    JSON* protectionRulesJSON = JSONGetObjectItem(protectionJSON, "ProtectionRules");
    if (protectionRulesJSON == NULL)
    {
        return 1;
    }

    Protectionrules.count = JSONGetArraySize(protectionRulesJSON);
    Protectionrules.rules = malloc(sizeof(*Protectionrules.rules)*Protectionrules.count);
    if (Protectionrules.rules == NULL)
    {
        return 1;
    }

    JSON* protectionRuleJSON = NULL;
    int currentIndex = 0;
    JSONArrayForEach(protectionRuleJSON, protectionRulesJSON)
    {
        JSON* expressionJSON = JSONGetObjectItem(protectionRuleJSON, "Expression");
        JSON* errorMessageJSON = JSONGetObjectItem(protectionRuleJSON, "ErrorMessage");
        JSON* errorCodeJSON = JSONGetObjectItem(protectionRuleJSON, "ErrorCode");
        char* expressionString = expressionJSON->valuestring;
        Variable* variables = malloc(sizeof(*variables)*(sensorCount+actuatorCount));
        for (int i = 0; i < sensorCount; i++)
        {
            variables[i] = (Variable){sensors[i].sensorID, &sensors[i].value};
        }
        for (int i = sensorCount; i < sensorCount+actuatorCount; i++)
        {
            unsigned int k = i - sensorCount;
            variables[k] = (Variable){actuators[k].actuatorID, &actuators[k].value};
        }
        Protectionrules.rules[currentIndex].expression = parseBooleanExpression(expressionString, strlen(expressionString), variables, sensorCount+actuatorCount);
        free(variables);
        
        /* find out what kind of fault/error the protection rule would trigger */
        if (strchr(expressionString, SENSOR_PREFIX) != NULL)
        {
            if (strchr(expressionString, ACTUATOR_PREFIX) != NULL)
            {
                Protectionrules.rules[currentIndex].errorType = DELAY_ERROR;
            }
            else
            {
                Protectionrules.rules[currentIndex].errorType = INFRASTRUCTURE_ERROR;
            }
        }
        else if (strchr(expressionString, ACTUATOR_PREFIX) != NULL)
        {
            Protectionrules.rules[currentIndex].errorType = USER_ERROR;
        }
        else
        {
            //TODO handle incorrect syntax of protectionRule
        }

        Protectionrules.rules[currentIndex].errorMessage = malloc(strlen(errorMessageJSON->valuestring)+1);
        if (Protectionrules.rules[currentIndex].errorMessage == NULL)
        {
            return 1;
        }
        memcpy(Protectionrules.rules[currentIndex].errorMessage, errorMessageJSON->valuestring, strlen(errorMessageJSON->valuestring)+1);
        Protectionrules.rules[currentIndex].errorCode = errorCodeJSON->valueint;
        currentIndex++;
    }

    JSONDelete(protectionJSON);

    for (int i = 0; i < Protectionrules.count; i++)
    {
        printProtectionRule(Protectionrules.rules[i]);
    }

    return 0;
}

static void startPhysicalSystem(void)
{
    //TODO maybe add check for protectionrules
    stoppedPS = 0;
}

static void stopPhysicalSystem(void)
{
    pthread_mutex_lock(&mutexSPI);
    stoppedPS = 1;
    for (int i = 0; i < actuatorCount; i++)
    {
        spiAnswer answer = executeSPICommand(actuators[i].command, actuators[i].stopData);
        free(answer.answer);
    }
    pthread_mutex_unlock(&mutexSPI);
}

static void handleDelayBasedFault(DelayBasedFault* fault)
{
    for (int i = 0; i < 100; i++)
    {
        usleep(100);
        if (!evaluateBooleanExpression(fault->rule->expression))
        {
            fault->isActive = 0;
            return;
        }
    }
    fault->isActive = 0;
    JSON* delayBasedErrorJSON = JSONCreateObject();
    JSONAddNumberToObject(delayBasedErrorJSON, "ErrorCode", fault->rule->errorCode);
    char* delayBasedError = JSONPrint(delayBasedErrorJSON);
    sendMessageIPC(communicationService, IPCMSGTYPE_DELAYBASEDERROR, delayBasedError, strlen(delayBasedError));

    JSONDelete(delayBasedErrorJSON);
    free(delayBasedError);
}

static int messageHandlerCommunicationService(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            while(hasMessages(ipcsc))
            {
                Message msg = receiveMessageIPC(ipcsc);
                //printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_INITPROTECTIONSERVICE:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSON* sensorsJSON = JSONGetObjectItem(msgJSON, "Sensors");
                        JSON* actuatorsJSON = JSONGetObjectItem(msgJSON, "Actuators");
                        JSON* protectionRulesJSON = JSONGetObjectItem(msgJSON, "ProtectionRules");
                        char* stringSensors = JSONPrint(sensorsJSON);
                        char* stringActuators = JSONPrint(actuatorsJSON);
                        char* stringProtectionRules = JSONPrint(protectionRulesJSON);

                        sensors = parseSensors(stringSensors, strlen(stringSensors), &sensorCount);
                        if (sensors == NULL)
                        {
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < sensorCount; i++)
                            {
                                printSensorData(sensors[i]);
                            }
                        }

                        incomingActuators = parseActuators(stringActuators, strlen(stringActuators), &actuatorCount);
                        actuators = malloc(sizeof(*actuators)*actuatorCount);
                        *actuators = *incomingActuators;
                        if (incomingActuators == NULL)
                        {
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < actuatorCount; i++)
                            {
                                printActuatorData(incomingActuators[i]);
                            }
                        }

                        if (parseProtectionRules(stringProtectionRules))
                        {
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        delayBasedFaults.maxAmount = 0;
                        for (int i = 0; i < Protectionrules.count; i++)
                        {
                            if (Protectionrules.rules[i].errorType == DELAY_ERROR)
                            {
                                delayBasedFaults.maxAmount++;
                            }
                        }
                        delayBasedFaults.faults = malloc(sizeof(*delayBasedFaults.faults)*delayBasedFaults.maxAmount);
                        unsigned int currentFaultIndex = 0;
                        for (int i = 0; i < Protectionrules.count; i++)
                        {
                            if (Protectionrules.rules[i].errorType == DELAY_ERROR)
                            {
                                delayBasedFaults.faults[currentFaultIndex].isActive = 0;
                                delayBasedFaults.faults[currentFaultIndex].rule = &Protectionrules.rules[i];
                                currentFaultIndex++;
                            }
                        }

                        char* result = serializeInt(1);
                        sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                        free(result);

                        JSONDelete(msgJSON);
                        free(stringSensors);
                        free(stringActuators);
                        free(stringProtectionRules);
                        break;
                    }

                    case IPCMSGTYPE_SETUSERVARIABLE:
                    {
                        //TODO maybe rename variables as these are still the names of the old system and maybe change their content too
                        JSON* msgJSON = JSONParse(msg.content);
                        unsigned int userVariable = JSONGetObjectItem(msgJSON, "Variable"); //the index of the virtual sensor (looking only at the virtual sensors)
                        long long value = JSONGetObjectItem(msgJSON, "State");              //the new value of the virtual sensor
                        unsigned int virtualIndex = 0;                                      //keeps track of the number of virtual sensors we have already seen
                        for (int i = 0; i < sensorCount; i++)
                        {
                            if (sensors[i].isVirtual && virtualIndex == userVariable)
                            {
                                sensors[i].value = value;
                                sensors[i].valueDouble = (double)value;
                            }
                            else if (sensors[i].isVirtual)
                            {
                                virtualIndex++;
                            }
                        }
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_ACTUATORDATA:
                    {
                        unsigned int newActuatorDataCount = 0;
                        ActuatorDataPacket* actuatorDataPackets = parseActuatorDataPackets(msg.content, msg.length, &newActuatorDataCount);
                        if (actuatorDataPackets == NULL)
                        {
                            //TODO error handling
                        }
                        else
                        {
                            for (int i = 0; i < newActuatorDataCount; i++)
                            {
                                Actuator* actuator = getActuatorWithID(incomingActuators, actuatorDataPackets[i].actuatorID, actuatorCount);
                                actuator->value = actuatorDataPackets[i].value;
                                actuator->valueDouble = (double)actuatorDataPackets[i].value;
                                free(actuatorDataPackets[i].actuatorID);
                            }
                        }
                        free(actuatorDataPackets);
                        break;
                    }

                    case IPCMSGTYPE_RUNPHYSICALSYSTEM:
                    {
                        startPhysicalSystem();
                        break;
                    }

                    case IPCMSGTYPE_STOPPHYSICALSYSTEM:
                    {
                        stopPhysicalSystem();
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
    pthread_mutex_init(&mutexSPI, NULL);

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
    
        /* Poll the new sensor values and forward them to communication service if the value changed */
        if (!stoppedPS)
        {
            long long oldSensorValues[sensorCount];
            //TODO check for NULL?
            JSON* sensorDataMsgJSON = JSONCreateObject();
            JSON* sensorDataJSON = JSONCreateArray();
            for (int i = 0; i < sensorCount; i++)
            {
                oldSensorValues[i] = sensors[i].value;
                pthread_mutex_lock(&mutexSPI);
                spiAnswer answer = executeSPICommand(sensors[i].command, NULL);
                pthread_mutex_unlock(&mutexSPI);
                SPIAnswerToSensorValue(&sensors[i], answer);
                if(answer.answerLength > 0)
                {
                    free(answer.answer);
                }
                if (oldSensorValues[i] != sensors[i].value)
                {
                    SensorDataPacket packet = (SensorDataPacket){sensors[i].sensorID, sensors[i].value};
                    JSON* packetJSON = SensorDataPacketToJSON(packet);
                    JSONAddItemToArray(sensorDataJSON, packetJSON);
                }
            }
            JSONAddItemToObject(sensorDataMsgJSON, "SensorData", sensorDataJSON);
            char* sensorDataMsg = JSONPrint(sensorDataMsgJSON);
            sendMessageIPC(communicationService, IPCMSGTYPE_SENSORDATA, sensorDataMsg, strlen(sensorDataMsg));
            JSONDelete(sensorDataMsgJSON);
            free(sensorDataMsg);
        }

        *actuators = *incomingActuators;

        if (!stoppedPS)
        {
            /* Check the Protection rules */
            for (int i = 0; i < Protectionrules.count; i++)
            {
                if (evaluateBooleanExpression(&Protectionrules.rules[i]))
                {
                    if (!stoppedPS)
                    {
                        stopPhysicalSystem();
                    }
                    switch (Protectionrules.rules[i].errorType)
                    {
                        case DELAY_ERROR:
                        {
                            for (int j = 0; j < delayBasedFaults.maxAmount; j++)
                            {
                                if (delayBasedFaults.faults[j].rule->errorCode == Protectionrules.rules[i].errorCode)
                                {
                                    if (delayBasedFaults.faults[j].isActive)
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        pthread_create(delayBasedFaults.faults[j].thread, NULL, &handleDelayBasedFault, &delayBasedFaults.faults[j]);
                                        pthread_detach(delayBasedFaults.faults[j].thread);
                                    }
                                }
                            }

                            JSON* delayFaultMsgJSON = JSONCreateObject();
                            JSONAddNumberToObject(delayFaultMsgJSON, "FaultID", Protectionrules.rules[i].errorCode);
                            JSON* sensorDataJSON = JSONCreateArray();
                            for (int i = 0; i < sensorCount; i++)
                            {
                                SensorDataPacket packet = (SensorDataPacket){sensors[i].sensorID, sensors[i].value};
                                JSON* packetJSON = SensorDataPacketToJSON(packet);
                                JSONAddItemToArray(sensorDataJSON, packetJSON);
                            }
                            JSONAddItemToObject(delayFaultMsgJSON, "SensorData", sensorDataJSON);
                            char* delayFaultMsg = JSONPrint(delayFaultMsgJSON);
                            sendMessageIPC(communicationService, IPCMSGTYPE_DELAYBASEDFAULT, delayFaultMsg, strlen(delayFaultMsg));
                            JSONDelete(delayFaultMsgJSON);
                            free(delayFaultMsg);
                            break;
                        }

                        case USER_ERROR:
                        {
                            JSON* userBasedErrorJSON = JSONCreateObject();
                            JSONAddNumberToObject(userBasedErrorJSON, "ErrorCode", Protectionrules.rules[i].errorCode);
                            char* userBasedError = JSONPrint(userBasedErrorJSON);
                            sendMessageIPC(communicationService, IPCMSGTYPE_USERBASEDERROR, userBasedError, strlen(userBasedError));
                            JSONDelete(userBasedErrorJSON);
                            free(userBasedError);
                            break;
                        }

                        case INFRASTRUCTURE_ERROR:
                        {
                            JSON* infrastructureBasedErrorJSON = JSONCreateObject();
                            JSONAddNumberToObject(infrastructureBasedErrorJSON, "ErrorCode", Protectionrules.rules[i].errorCode);
                            char* infrastructureBasedError = JSONPrint(infrastructureBasedErrorJSON);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INFRASTRUCTUREBASEDERROR, infrastructureBasedError, strlen(infrastructureBasedError));
                            JSONDelete(infrastructureBasedErrorJSON);
                            free(infrastructureBasedError);
                            break;
                        }

                        default:
                        {
                            break;
                        }
                    }
                }
            }

            /* Send the CurrentActuator values to the FPGA */
            for (int i = 0; i < actuatorCount; i++)
            {
                spiAnswer answer;
                pthread_mutex_lock(&mutexSPI);
                if (stoppedPS)
                {
                    pthread_mutex_unlock(&mutexSPI);
                    break;
                }
                char * data = ActuatorValueToSPIData(&actuators[i]);
                answer = executeSPICommand(actuators[i].command, data);
                pthread_mutex_unlock(&mutexSPI);
                free(data);
                free(answer.answer);
            }
        }
    }
    
    /* cleanup */
    for (int i = 0; i < Protectionrules.count; i++)
    {
        destroyBooleanExpression(Protectionrules.rules[i].expression);
        free(Protectionrules.rules[i].errorMessage);
        free(Protectionrules.rules);
    }
    destroySensors(sensors, sensorCount);
    destroyActuators(incomingActuators, actuatorCount);
    pthread_mutex_destroy(&mutexSPI);
    free(actuators);
    return 0;
}
