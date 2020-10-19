#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"
#include "utils/utils.h"

#define PHYSICAL_SYSTEM

typedef struct
{
    BooleanExpression*  expression;
    char*               errorMessage;
    int                 errorCode;
} Protectionrule;

static IPCSocketConnection* communicationService;
Sensor* sensors;
Actuator* actuators;
struct 
{
    Protectionrule* rules;
    int             count;
} Protectionrules;

void printProtectionRule(Protectionrule rule)
{
    printf("Expression: ");
    printBooleanExpression(rule.expression);
    printf("ErrorMessage: %s\n", rule.errorMessage);
    printf("ErrorCode: %d\n", rule.errorCode);
}

int parseProtectionRulesFromFile(char *filename)
{
    char* protectionString = readFile(filename);
    JSON* protectionJSON = JSONParse(protectionString);
    free(protectionString);
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
        Protectionrules.rules[currentIndex].expression = parseBooleanExpression(expressionString, strlen(expressionString), NULL, 0);
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
    /*int fd = createIPCSocket(PROTECTION_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }*/

    //while(1)
    //{
        /* Read all IPC Messages and update CurrentActuator */
        //while (hasMessages(communicationService));
    
        /* Save the Sensorvalues in CurrentSensor */

        /* Check the Protection rules */

        /* Send the CurrentActuator values to the FPGA */

    //}

    //pthread_join(communicationService->thread, NULL);
    /*char* filename = "../../../Testfiles/Protection.json";
    parseProtectionRulesFromFile(filename);
    for (int i = 0; i < Protectionrules.count; i++)
    {
        destroyBooleanExpression(Protectionrules.rules[i].expression);
        free(Protectionrules.rules[i].errorMessage);
        free(Protectionrules.rules);
    }*/
    char* filename = "../../../Testfiles/SensorsActuators.json";
    char* filecontent = readFile(filename);
    JSON* json = JSONParse(filecontent);
    int sensorCount = JSONGetArraySize(JSONGetObjectItem(json, "Sensors"));
    Sensor* sensors = parseSensors(filecontent, strlen(filecontent));
    for (int i = 0; i < sensorCount; i++)
    {
        printSensorData(sensors[i]);
    }
    int actuatorCount = JSONGetArraySize(JSONGetObjectItem(json, "Actuators"));
    Actuator* actuators = parseActuators(filecontent, strlen(filecontent));
    for (int i = 0; i < actuatorCount; i++)
    {
        printActuatorData(actuators[i]);
    }
    JSONDelete(json);
    free(filecontent);
    return 0;
}
