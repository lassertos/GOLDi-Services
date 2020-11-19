#define SENSOR_PREFIX 'x'
#define ACTUATOR_PREFIX 'y'
#define EXTENDED_SENSORS_ACTUATORS

#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"
#include "utils/utils.h"
#include "logging/log.h"

/* all possible error types */
typedef enum
{
    DELAY_ERROR,
    USER_ERROR,
    INFRASTRUCTURE_ERROR
} ErrorType;

/*
 *  Protectionrules can be checked to test if faults/errors have occured 
 *  expression      -   this is evaluated during the main loop, an error has occurred 
 *                      when the expression evaluates to 1
 *  errorType       -   the type of the error
 *  errorMessage    -   a string containing a descriptive error Message
 *  errorCode       -   the internal code of the error
 */
typedef struct
{
    BooleanExpression*  expression;
    ErrorType           errorType;
    char*               errorMessage;
    int                 errorCode;
} Protectionrule;

/* a struct containing all the Protectionrules and their count */
struct 
{
    Protectionrule* rules;
    int             count;
} Protectionrules;

/*
 *  DelayBasedFault struct to keep track of a specific delay based fault
 *  rule        -   the associated Protectionrule which is checked continously and at the end
 *  thread      -   the associated thread where the DelayBasedFault is handled in
 *  isActive    -   indicates whether the thread is still active
 */
typedef struct
{
    Protectionrule* rule;
    pthread_t       thread;
    unsigned int    isActive:1;
} DelayBasedFault;

/* a struct containing all the possible DelayBasedFaults and their maximum count */
struct 
{
    DelayBasedFault*    faults;
    unsigned int        maxCount;
} delayBasedFaults;

/* global variables needed for execution */
static IPCSocketConnection* communicationService;   // the IPC-socket to the Communication Service
static Sensor* sensors;                             // here all of our sensor data is saved
static Actuator* actuators;                         // here all of our actuator data is saved
static Actuator* incomingActuators;                 // this is used for buffering new actuator data
static unsigned int sensorCount;                    // the amount of sensors
static unsigned int actuatorCount;                  // the amount of actuators
static unsigned int stoppedPS = 1;                  // indicates whether the physical system has been stopped
static pthread_mutex_t mutexSPI;                    // used to coordinate spi access
static volatile unsigned int initialized = 0;       // used to indicate whether the service has been initialized

/*
 * the sigint handler, can also be used for cleanup after execution 
 * sig - the signal the program received, can be ignored for our purposes
 */
static void sigint_handler(int sig)
{
    exit(0);
}

/*
 *  can be used to print out the information of a Protectionrule
 *  rule    -   the Protectionrule to be printed 
 */
void printProtectionRule(Protectionrule rule)
{
    printf("Expression: ");
    printBooleanExpression(rule.expression);
    printf("ErrorMessage: %s\n", rule.errorMessage);
    printf("ErrorCode: %d\n", rule.errorCode);
}

/*
 *  used to parse the Protectionrules given by the Communication Service as a JSON-formatted string
 *  protectionString    -   the JSON-formatted string containing all Protectionrules 
 */
int parseProtectionRules(char *protectionString)
{
    JSON* protectionJSON = JSONParse(protectionString);
    if (protectionJSON == NULL)
    {
        log_error("parse protection: string could not be parsed to json");
        return 1;
    }

    JSON* protectionRulesJSON = JSONGetObjectItem(protectionJSON, "ProtectionRules");
    if (protectionRulesJSON == NULL)
    {
        log_error("parse protection: protection could not be accessed in json");
        return 1;
    }

    Protectionrules.count = JSONGetArraySize(protectionRulesJSON);
    Protectionrules.rules = malloc(sizeof(*Protectionrules.rules)*Protectionrules.count);
    if (Protectionrules.rules == NULL)
    {
        log_error("parse protection: malloc error %s", strerror(errno));
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
            //TODO error handling: handle incorrect syntax of protectionRule
        }

        Protectionrules.rules[currentIndex].errorMessage = malloc(strlen(errorMessageJSON->valuestring)+1);
        if (Protectionrules.rules[currentIndex].errorMessage == NULL)
        {
            log_error("parse protection: malloc error %s", strerror(errno));
            return 1;
        }
        memcpy(Protectionrules.rules[currentIndex].errorMessage, errorMessageJSON->valuestring, strlen(errorMessageJSON->valuestring)+1);
        Protectionrules.rules[currentIndex].errorCode = errorCodeJSON->valueint;
        currentIndex++;
    }

    JSONDelete(protectionJSON);

    for (int i = 0; i < Protectionrules.count; i++)
    {
        //printProtectionRule(Protectionrules.rules[i]);
    }

    return 0;
}

/* used to start the physical system */
static void startPhysicalSystem(void)
{
    //TODO maybe add check for protectionrules
    stoppedPS = 0;
}

/* used to stop the physical system */
static void stopPhysicalSystem(void)
{
    stoppedPS = 1;
    for (int i = 0; i < actuatorCount; i++)
    {
        spiAnswer answer = executeSPICommand(actuators[i].command, actuators[i].stopData, &mutexSPI);
        free(answer.content);
    }
}

/*
 *  the handler for DelayBasedFaults, it checks periodically for 10 seconds if the fault has resolved 
 *  and stops if so. If the fault has not been resolved after 10 seconds an error message will be send
 *  to the Communication Service
 *  fault   -   the DelayBasedFault to be handled 
 */
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

/*
 *  a message handler for the IPC-sockets
 *  ipcsc   -   the IPCSocketConnection to be handled
 */
static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            while(hasMessages(ipcsc))
            {
                log_debug("receiving IPC message");
                Message msg = receiveMessageIPC(ipcsc);
                //log_debug("\nMESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s", msg.type, msg.length, msg.content);
                switch (msg.type)
                {
                    case IPCMSGTYPE_INITPROTECTIONSERVICE:
                    {
                        log_debug("initialization: starting initialization");
                        log_debug("initialization: parsing message content to json");
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
                        log_debug("initialization: parsing protection as json from message json");
                        JSON* protectionRulesJSON = JSONGetObjectItem(msgJSON, "ProtectionRules");
                        if (protectionRulesJSON == NULL)
                        {
                            log_error("initialization: protection not included in message json");
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
                        log_debug("initialization: converting protection json to string");
                        char* stringProtectionRules = JSONPrint(protectionRulesJSON);
                        if (stringProtectionRules == NULL)
                        {
                            log_error("initialization: protection json could not be parsed to string");
                            break;
                        }

                        JSONDelete(msgJSON);

                        log_debug("initialization: parsing sensors");
                        sensors = parseSensors(stringSensors, strlen(stringSensors), &sensorCount);
                        free(stringSensors);
                        if (sensors == NULL)
                        {
                            log_error("initialization: sensors could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < sensorCount; i++)
                            {
                                //printSensorData(sensors[i]);
                            }
                        }

                        log_debug("initialization: parsing actuators");
                        incomingActuators = parseActuators(stringActuators, strlen(stringActuators), &actuatorCount);
                        actuators = malloc(sizeof(*actuators)*actuatorCount);
                        *actuators = *incomingActuators;
                        free(stringActuators);
                        if (incomingActuators == NULL)
                        {
                            log_error("initialization: actuators could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            break;
                        }
                        else
                        {
                            for (int i = 0; i < actuatorCount; i++)
                            {
                                //printActuatorData(incomingActuators[i]);
                            }
                        }

                        log_debug("initialization: parsing protection");
                        if (parseProtectionRules(stringProtectionRules))
                        {
                            log_error("initialization: protection could not be parsed successfully");
                            char* result = serializeInt(0);
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                            free(result);
                            free(stringProtectionRules);
                            break;
                        }
                        free(stringProtectionRules);
                        delayBasedFaults.maxCount = 0;
                        for (int i = 0; i < Protectionrules.count; i++)
                        {
                            if (Protectionrules.rules[i].errorType == DELAY_ERROR)
                            {
                                delayBasedFaults.maxCount++;
                            }
                        }
                        delayBasedFaults.faults = malloc(sizeof(*delayBasedFaults.faults)*delayBasedFaults.maxCount);
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

                        log_debug("initialization: sending result to Communication Service");
                        char* result = serializeInt(1);
                        sendMessageIPC(communicationService, IPCMSGTYPE_INITPROTECTIONFINISHED, result, 4);
                        free(result);
                        initialized = 1;

                        break;
                    }

                    case IPCMSGTYPE_SETUSERVARIABLE:
                    {
                        //TODO maybe rename variables as these are still the names of the old system and maybe change their content too
                        JSON* msgJSON = JSONParse(msg.content);
                        unsigned int userVariable = JSONGetObjectItem(msgJSON, "Variable")->valueint;   //the index of the virtual sensor (looking only at the virtual sensors)
                        long long value = JSONGetObjectItem(msgJSON, "State")->valueint;                //the new value of the virtual sensor
                        unsigned int virtualIndex = 0;                                                  //keeps track of the number of virtual sensors we have already seen
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

                    case IPCMSGTYPE_EXPERIMENTINIT:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSON* packetsJSON = JSONCreateArray();
                        SensorDataPacket* packets = malloc(sizeof(*packets)*sensorCount);

                        for (int i = 0; i < sensorCount; i++)
                        {
                            packets[i] = (SensorDataPacket){sensors[i].sensorID, sensors[i].value};
                            JSONAddItemToArray(packetsJSON, SensorDataPacketToJSON(packets[i]));
                        }
                        
                        free(packets);
                        JSONAddItemToObject(msgJSON, "SensorData", packetsJSON);
                        
                        char* message = JSONPrint(msgJSON);
                        sendMessageIPC(communicationService, IPCMSGTYPE_EXPERIMENTINIT, message, strlen(message));
                        free(message);

                        JSONDelete(msgJSON);
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
    /* initialize the mutex and all needed sockets */
    pthread_mutex_init(&mutexSPI, NULL);

    if (setupSPIInterface())
    {
        return -1;
    }

    int fd = createIPCSocket(PROTECTION_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    while(!initialized);

    while(1)
    {
        log_info("waiting for all messages to be read");
        /* Read all IPC Messages and update CurrentActuator */
        while (hasMessages(communicationService));
    
        if (stoppedPS)
        {
            log_info("skipping polling of sensor values");
        }
        else
        {
            log_info("polling sensor values");
        }

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
                char data = sensors[i].pinMapping;
                //TODO update once spi changes have been made
                spiAnswer answer = executeSPICommand(SPICOMMAND_READ_GPIO, &data, &mutexSPI);
                SPIAnswerToSensorValue(&sensors[i], answer);
                if(answer.length > 0)
                {
                    free(answer.content);
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

        if (stoppedPS)
        {
            log_info("skipping checking of protection");
        }
        else
        {
            log_info("checking protection");
        }

        if (!stoppedPS)
        {
            /* Check the Protection rules */
            for (int i = 0; i < Protectionrules.count; i++)
            {
                if (evaluateBooleanExpression(Protectionrules.rules[i].expression))
                {
                    if (!stoppedPS)
                    {
                        stopPhysicalSystem();
                    }
                    switch (Protectionrules.rules[i].errorType)
                    {
                        case DELAY_ERROR:
                        {
                            for (int j = 0; j < delayBasedFaults.maxCount; j++)
                            {
                                if (delayBasedFaults.faults[j].rule->errorCode == Protectionrules.rules[i].errorCode)
                                {
                                    if (delayBasedFaults.faults[j].isActive)
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        pthread_create(&delayBasedFaults.faults[j].thread, NULL, &handleDelayBasedFault, &delayBasedFaults.faults[j]);
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
            log_info("sending new actuator values to fpga");
            for (int i = 0; i < actuatorCount; i++)
            {
                spiAnswer answer;
                if (stoppedPS)
                {
                    log_info("stopping sending of actuator values to fpga");
                    break;
                }
                //TODO update once spi changes have been made
                char* data = ActuatorValueToSPIData(&actuators[i]);
                char* completeData = malloc(SPICOMMAND_WRITE_GPIO.dataLength);
                completeData[0] = actuators[i].pinMapping;
                memcpy(completeData+1, data, SPICOMMAND_WRITE_GPIO.dataLength - 1);
                answer = executeSPICommand(SPICOMMAND_WRITE_GPIO, data, &mutexSPI);
                free(data);
                free(answer.content);
            }
        }
    }
    
    /* cleanup */
    log_info("execution finished, cleaning up");
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
    closeSPIInterface();
    closeIPCConnection(communicationService);
    return 0;
}
