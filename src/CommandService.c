#include "interfaces/ipcsockets.h"
#include "interfaces/spi.h"
#include "interfaces/SensorsActuators.h"
#include "logging/log.h"
#include "utils/utils.h"

static IPCSocketConnection* communicationService;
static Sensor* sensors;
static Actuator* actuators;
static unsigned int sensorCount;
static unsigned int actuatorCount;
static unsigned int initialized = 0;
static unsigned int stopped = 0;
static pthread_mutex_t mutexSPI;

/*
 * the sigint handler, can also be used for cleanup after execution 
 * sig - the signal the program received, can be ignored for our purposes
 */
static void sigint_handler(int sig)
{
    exit(0);
}

/*static int sendSensorValues(SensorDataPacket* packets, unsigned int packetcount)
{
    for (int i = 0; i < packetcount; i++)
    {
        Sensor* sensor = getSensorWithID(sensors, packets[i].sensorID, sensorCount);
        if (sensor->type == SensorTypeBinary)
        {
            char* data = malloc(2);
            if (data == NULL)
            {
                //TODO error handling
                return 1;
            }
            data[0] = sensor->pinMapping;
            if (!sensor->value)
            {
                data[1] = 0x00;
            }
            else
            {
                data[1] = 0x80;
            }
            executeSPICommand(SPICOMMAND_WRITE_GPIO, data);
            free(data);
        }
    }
    return 0;
}*/

//TODO update once spi changes have been made
static int sendSensorValue(Sensor* sensor)
{
    if (stopped)
    {
        return 0;
    }
    switch (sensor->type)
    {
        case SensorTypeBinary:
        {
            char* data = malloc(2);
            if (data == NULL)
            {
                //TODO error handling
                log_error("malloc error: %s", strerror(errno));
                return 1;
            }
            data[0] = sensor->pinMapping;
            if (!sensor->value)
            {
                data[1] = 0x00;
            }
            else
            {
                data[1] = 0x01;
            }
            executeSPICommand(SPICOMMAND_WRITE_GPIO, data, &mutexSPI);
            free(data);
            return 0;
        }
        default:
        {
            return 1;
        }
    }
}

static ActuatorDataPacket* retrieveActuatorValues(unsigned int* packetcount)
{
    if (stopped)
    {
        return NULL;
    }
    ActuatorDataPacket* packets = malloc(sizeof(*packets) * actuatorCount);
    *packetcount = 0;
    for (int i = 0; i < actuatorCount; i++)
    {
        switch (actuators[i].type)
        {
            case ActuatorTypeBinary:
            {   
                long long oldValue = actuators[i].value;
                char data = i;
                spiAnswer answer = executeSPICommand(SPICOMMAND_READ_GPIO, &data, &mutexSPI);
                actuators[i].value = answer.content[0];
                free(answer.content);
                if (oldValue != actuators[i].value)
                {
                    packets[*packetcount] = (ActuatorDataPacket){actuators[i].actuatorID, actuators[i].value};
                    (*packetcount)++;
                }
                break;
            }
            
            default:
            {
                break;
            }
        }
    }

    if (*packetcount > 0)
    {
        return packets;
    }
    else
    {
        free(packets);
        return NULL;
    }
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
                    case IPCMSGTYPE_SENSORDATA:
                    {
                        log_debug("received sensor data message");
                        unsigned int newSensorDataCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(msg.content, msg.length, &newSensorDataCount);
                        for (int i = 0; i < newSensorDataCount; i++)
                        {
                            Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                            sensor->value = sensorDataPackets[i].value;
                            free(sensorDataPackets[i].sensorID);
                            sendSensorValue(sensor);
                        }
                        free(sensorDataPackets);
                        break;
                    }
                    
                    case IPCMSGTYPE_INITCOMMANDSERVICE:
                    {
                        log_debug("received initialization message");
                        //TODO check for NULL?
                        JSON* msgJSON = JSONParse(msg.content);
                        if (msgJSON == NULL)
                        {
                            //TODO error handling
                            log_error("message could not be parsed to json object");
                        }

                        JSON* experimentDataJSON = JSONGetObjectItem(msgJSON, "Experiment");
                        JSON* sensorValuesJSON = JSONGetObjectItem(msgJSON, "SensorData");
                        if (experimentDataJSON == NULL || sensorValuesJSON == NULL)
                        {
                            //TODO error handling
                            log_error("experiment data or sensor values could not be retrieved from the message");
                        }

                        JSON* sensorsJSON = JSONGetObjectItem(experimentDataJSON, "Sensors");
                        JSON* actuatorsJSON = JSONGetObjectItem(experimentDataJSON, "Actuators");
                        if (sensorsJSON == NULL || actuatorsJSON == NULL)
                        {
                            //TODO error handling
                            log_error("sensor data or actuator data could not be retrieved from the message");
                        }

                        /* initialize sensors object */
                        char* stringSensors = JSONPrint(sensorsJSON);
                        sensors = parseSensors(stringSensors, strlen(stringSensors), &sensorCount);
                        free(stringSensors);
                        if (sensors == NULL)
                        {
                            sendMessageIPC(ipcsc, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                        }

                        /* initialize actuators object */
                        char* stringActuators = JSONPrint(actuatorsJSON);
                        actuators = parseActuators(stringActuators, strlen(stringActuators), &actuatorCount);
                        free(stringActuators);
                        if (actuators == NULL)
                        {
                            sendMessageIPC(ipcsc, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                        }

                        /* initialize sensor values */
                        if (!JSONIsNull(sensorValuesJSON))
                        {
                            char* stringSensorValues = JSONPrint(sensorValuesJSON);
                            unsigned int sensorDataPacketsCount = 0;
                            SensorDataPacket* sensorDataPackets = parseSensorDataPackets(stringSensorValues, strlen(stringSensorValues), &sensorDataPacketsCount); 
                            free(stringSensorValues);

                            if (sensorCount != sensorDataPacketsCount)
                            {
                                sendMessageIPC(ipcsc, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, NULL, 0);
                            }

                            for (int i = 0; i < sensorCount; i++)
                            {
                                Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                                sensor->value = sensorDataPackets[i].value;
                                free(sensorDataPackets[i].sensorID);
                                sendSensorValue(sensor);
                            }

                            free(sensorDataPackets);
                        }

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
                        sendMessageIPC(ipcsc, IPCMSGTYPE_INITCOMMANDSERVICEFINISHED, finishedMessage, strlen(finishedMessage));
                        free(finishedMessage);

                        initialized = 1;

                        /* cleanup */
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_DELAYBASEDFAULT:
                    {
                        log_debug("received delay fault message");
                        JSON* msgJSON = JSONParse(msg.content);
                        JSON* sensorDataPacketsJSON = JSONGetObjectItem(msgJSON, "SensorData");
                        char* stringSensorDataPackets = JSONPrint(sensorDataPacketsJSON);
                        unsigned int sensorDataPacketCount = 0;
                        SensorDataPacket* sensorDataPackets = parseSensorDataPackets(stringSensorDataPackets, strlen(stringSensorDataPackets), &sensorDataPacketCount);
                        if (sensorDataPacketCount != sensorCount)
                        {
                            //TODO add error handling
                            log_error("did not receive current data for all sensors");
                        }
                        for (int i = 0; i < sensorDataPacketCount; i++)
                        {
                            Sensor* sensor = getSensorWithID(sensors, sensorDataPackets[i].sensorID, sensorCount);
                            sensor->value = sensorDataPackets[i].value;
                            sendSensorValue(sensor);
                            free(sensorDataPackets[i].sensorID);
                        }

                        /* this is only used for fetching the new actuator values */
                        unsigned int packetcount;
                        ActuatorDataPacket* packets = retrieveActuatorValues(&packetcount);
                        free(packets);

                        /* prepare actuator data of all actuators for delayFaultAck */
                        JSON* actuatorDataJSON = JSONCreateArray();
                        for (int i = 0; i < actuatorCount; i++)
                        {
                            ActuatorDataPacket packet = {actuators[i].actuatorID, actuators[i].value};
                            JSONAddItemToArray(actuatorDataJSON, ActuatorDataPacketToJSON(packet));
                        }

                        JSONDeleteItemFromObject(msgJSON, "SensorData");
                        JSONAddItemToObject(msgJSON, "ActuatorData", actuatorDataJSON);

                        char* messageDelayFaultAck = JSONPrint(msgJSON);
                        sendMessageIPC(ipcsc, IPCMSGTYPE_DELAYBASEDFAULTACK, messageDelayFaultAck, strlen(messageDelayFaultAck));

                        free(messageDelayFaultAck);
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_PROGRAMCONTROLUNIT:
                    {
                        log_debug("received program control unit message");
                        stopped = 1;
                        JSON* msgJSON = JSONCreateObject();
                        JSON* actuatorDataJSON = JSONAddArrayToObject(msgJSON, "ActuatorData");
                        ActuatorDataPacket* packets = malloc(sizeof(*packets) * actuatorCount);

                        for (int i = 0; i < actuatorCount; i++)
                        {
                            actuators[i].value = actuators[i].stopValue;
                            packets[i] = (ActuatorDataPacket){actuators[i].actuatorID, actuators[i].value};
                        }

                        for(int i = 0; i < actuatorCount; i++)
                        {
                            JSONAddItemToArray(actuatorDataJSON, ActuatorDataPacketToJSON(packets[i]));
                        }

                        char* message = JSONPrint(msgJSON);
                        sendMessageIPC(ipcsc, IPCMSGTYPE_ACTUATORDATA, message, strlen(message));
                        
                        free(packets);
                        free(message);
                        JSONDelete(msgJSON);
                        break;
                    }

                    case IPCMSGTYPE_PROGRAMCONTROLUNITFINISHED:
                    {
                        log_debug("received program control unit finished message");
                        stopped = 0;
                        break;
                    }

                    case IPCMSGTYPE_ENDEXPERIMENT:
                    {
                        log_debug("received end experiment message");
                        initialized = 0;
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
}

int main(int argc, char const *argv[])
{
    if(setupSPIInterface() < 0)
    {
        log_error("setup of SPI-interface failed");
        return -1;
    }

    pthread_mutex_init(&mutexSPI, NULL);

    int fd = createIPCSocket(COMMAND_SERVICE);
    communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
    {
        log_error("connection to Communication Service could not be established");
        return -1;
    }

    while (1)
    {
        if (initialized && !stopped)
        {
            JSON* msgJSON = JSONCreateObject();
            JSON* actuatorDataJSON = JSONAddArrayToObject(msgJSON, "ActuatorData");
            unsigned int packetcount = 0;
            ActuatorDataPacket* packets = retrieveActuatorValues(&packetcount);

            for(int i = 0; i < packetcount; i++)
            {
                JSONAddItemToArray(actuatorDataJSON, ActuatorDataPacketToJSON(packets[i]));
            }

            char* message = JSONPrint(msgJSON);
            sendMessageIPC(communicationService, IPCMSGTYPE_ACTUATORDATA, message, strlen(message));
            
            free(packets);
            free(message);
            JSONDelete(msgJSON);
        }
    }

    pthread_join(communicationService->thread, NULL);

    pthread_mutex_destroy(&mutexSPI);
    closeSPIInterface();

    return 0;
}
