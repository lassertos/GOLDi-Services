#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"

static IPCSocketConnection* communicationService;
static Sensor* sensors;
static Actuator* actuators;
static unsigned int sensorCount;
static unsigned int actuatorCount;
static unsigned int initialized = 0;

//TODO add implementation of function
static int sendSensorValues(SensorDataPacket* packets)
{
    return 0;
}

//TODO add implementation of function
static ActuatorDataPacket* receiveActuatorValues(void)
{
    return NULL;
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
                    case IPCMSGTYPE_SENSORDATA:
                    {
                        unsigned int newSensorDataCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(msg.content, msg.length, &newSensorDataCount);
                        for (int i = 0; i < newSensorDataCount; i++)
                        {
                            Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                            sensor->value = sensorDataPackets[i].value;
                            free(sensorDataPackets[i].sensorID);
                        }
                        free(sensorDataPackets);
                        free(msg.content);
                        break;
                    }
                    
                    case IPCMSGTYPE_INITCOMMANDSERVICE:
                    {
                        //TODO check for NULL?
                        JSON* msgJSON = JSONParse(msg.content);
                        JSON* experimentDataJSON = JSONGetObjectItem(msgJSON, "Experiment");
                        JSON* sensorValuesJSON = JSONGetObjectItem(msgJSON, "SensorData");
                        JSON* sensorsJSON = JSONGetObjectItem(experimentDataJSON, "Sensors");
                        JSON* actuatorsJSON = JSONGetObjectItem(experimentDataJSON, "Actuators");

                        /* initialize sensors object */
                        char* stringSensors = JSONPrint(sensorsJSON);
                        sensors = parseSensors(stringSensors, strlen(stringSensors), &sensorCount);
                        free(stringSensors);
                        if (sensors == NULL)
                        {
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                        }

                        /* initialize actuators object */
                        char* stringActuators = JSONPrint(actuatorsJSON);
                        actuators = parseActuators(stringActuators, strlen(stringActuators), &actuatorCount);
                        free(stringActuators);
                        if (actuators == NULL)
                        {
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                        }

                        /* initialize sensor values */
                        char* stringSensorValues = JSONPrint(sensorValuesJSON);
                        unsigned int sensorDataPacketsCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(stringSensorValues, strlen(stringSensorValues), &sensorDataPacketsCount); 

                        if (sensorCount != sensorDataPacketsCount)
                        {
                            sendMessageIPC(communicationService, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                        }

                        for (int i = 0; i < sensorCount; i++)
                        {
                            Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                            sensor->value = sensorDataPackets[i].value;
                            free(sensorDataPackets[i].sensorID);
                        }

                        free(sensorDataPackets);

                        /* start main loop and update actuator values for the first time */

                        /* prepare and send initialization finished message */
                        JSONDeleteItemFromObject(msgJSON, "Experiment");
                        JSONDeleteItemFromObject(msgJSON, "SensorData");

                        ActuatorDataPacket* actuatorPackets = malloc(sizeof(*actuatorPackets)*actuatorCount);
                        JSON* actuatorDataJSON = JSONCreateArray();
                        for (int i = 0; i < actuatorCount; i++)
                        {
                            actuatorPackets[i].actuatorID = actuators[i].actuatorID;
                            actuatorPackets[i].value = actuators[i].value;
                            JSON* actuatorPacketJSON = ActuatorDataPacketToJSON(actuatorPackets[i]);
                            JSONAddItemToArray(actuatorDataJSON, actuatorPacketJSON);
                        }
                        JSONAddItemToObject(msgJSON, "ActuatorData", actuatorDataJSON);
                        
                        char* finishedMessage = JSONPrint(msgJSON);
                        sendMessageIPC(communicationService, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, finishedMessage, strlen(finishedMessage));

                        initialized = 1;

                        /* cleanup */
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_DELAYBASEDFAULT:
                    {
                        JSON* msgJSON = JSONParse(msg.content);
                        JSON* sensorDataPacketsJSON = JSONGetObjectItem(msgJSON, "SensorData");
                        char* stringSensorDataPackets = JSONPrint(sensorDataPacketsJSON);
                        unsigned int sensorDataPacketCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(stringSensorDataPackets, strlen(stringSensorDataPackets), &sensorDataPacketCount);
                        if (sensorDataPacketCount != sensorCount)
                        {
                            //TODO add error handling
                        }
                        for (int i = 0; i < sensorDataPacketCount; i++)
                        {
                            Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                            sensor->value = sensorDataPackets[i].value;
                            free(sensorDataPackets[i].sensorID);
                        }
                        /* TODO send and receive new sensor/actuator values */

                        /* TODO send message where SensorData is replaced with ActuatorData */
                    }

                    case IPCMSGTYPE_ENDEXPERIMENT:
                    {
                        destroySensors(sensors, sensorCount);
                        destroyActuators(actuators, actuatorCount);
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
    if(setupSPIInterface() < 0)
    {
        printf("Setup of SPI-interface failed!\n");
        return -1;
    }

    int fd = createIPCSocket(COMMAND_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    while (1)
    {
        if (initialized)
        {
            /* TODO send new sensor values */

            /* TODO read new actuator values */
        }
    }

    pthread_join(communicationService->thread, NULL);

    closeSPIInterface();

    return 0;
}
