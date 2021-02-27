#include "SensorsActuators.h"
#include "../logging/log.h"
#include <string.h>
#include <stdlib.h>

char* sensorTypeToString(SensorType sensorType) {
    switch (sensorType)
    {
        case SensorTypeBinary:
            return "binary";
        
        case SensorTypeAnalog:
            return "analog";

        case SensorTypeProtocol:
            return "protocol";

        default:
            return NULL;
    }
}

char* actuatorTypeToString(ActuatorType actuatorType) {
    switch (actuatorType)
    {
        case ActuatorTypeBinary:
            return "binary";
        
        case ActuatorTypeAnalog:
            return "analog";

        case ActuatorTypeProtocol:
            return "protocol";

        default:
            return NULL;
    }
}

SensorType parseSensorType(char* string)
{
    if(!strcmp(string,"binary"))
    {
        return SensorTypeBinary;
    }
    else if(!strcmp(string,"analog"))
    {
        return SensorTypeAnalog;
    }
    else if(!strcmp(string,"protocol"))
    {
        return SensorTypeProtocol;
    }
    else
    {
        return SensorTypeUnknown;
    }
}

ActuatorType parseActuatorType(char* string)
{
    if(!strcmp(string,"binary"))
    {
        return ActuatorTypeBinary;
    }
    else if(!strcmp(string,"analog"))
    {
        return ActuatorTypeAnalog;
    }
    else if(!strcmp(string,"protocol"))
    {
        return ActuatorTypeProtocol;
    } 
    else
    {
        return ActuatorTypeUnknown;
    }
}

JSON* beautifySensorValue(char* value, SensorType sensorType)
{
    JSON* valueJSON = JSONCreateObject();
    switch(sensorType) 
    {
        case SensorTypeBinary:
        {
            if (value[0] == 0)
            {
                JSONAddNumberToObject(valueJSON, "SensorValue", 0);
            } 
            else if (value[0] == 1) 
            {
                JSONAddNumberToObject(valueJSON, "SensorValue", 1);
            }
            break;
        }

        default:
            return NULL;
    }
    return valueJSON;
}

JSON* beautifyActuatorValue(char* value, ActuatorType actuatorType)
{
    JSON* valueJSON = JSONCreateObject();
    switch(actuatorType) 
    {
        case ActuatorTypeBinary:
        {
            if (value[0] == 0)
            {
                JSONAddNumberToObject(valueJSON, "SensorValue", 0);
            } 
            else if (value[0] == 1) 
            {
                JSONAddNumberToObject(valueJSON, "SensorValue", 1);
            }
            break;
        }

        default:
            return NULL;
    }
    return valueJSON;
}

char* unbeautifySensorValue(JSON* valueJSON, SensorType sensorType)
{
    char* value;
    switch (sensorType)
    {
        case SensorTypeBinary:
        {
            value = malloc(getValueSizeOfSensorType(SensorTypeBinary));
            value[0] = valueJSON->valueint;
            break;
        }
        
        default:
            return NULL;
    }
    return value;
}

char* unbeautifyActuatorValue(JSON* valueJSON, ActuatorType actuatorType)
{
    char* value;
    switch (actuatorType)
    {
        case ActuatorTypeBinary:
        {
            value = malloc(getValueSizeOfActuatorType(ActuatorTypeBinary));
            value[0] = valueJSON->valueint;
            break;
        }
        
        default:
            return NULL;
    }
    return value;
}

unsigned int getValueSizeOfSensorType(SensorType sensorType)
{
    switch (sensorType)
    {
        case SensorTypeBinary:
            return 1;
        
        default:
            return 0;
    }
}

unsigned int getValueSizeOfActuatorType(ActuatorType actuatorType)
{
    switch (actuatorType)
    {
        case ActuatorTypeBinary:
            return 1;
        
        default:
            return 0;
    }
}

Sensor* parseSensors(char* str, int length, unsigned int* sensorCount)
{
    Sensor* sensors;
    JSON* jsonSensors = JSONParseWithLength(str, length);
    if (jsonSensors == NULL)
    {
        log_error("parsing of sensor data json failed");
        return NULL;
    }
    *sensorCount = JSONGetArraySize(jsonSensors);
    sensors = malloc(sizeof(*sensors)*(*sensorCount));
    if (sensors == NULL)
    {
        log_error("malloc error: %s", strerror(errno));
        return NULL;
    }
    const JSON* jsonSensor = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "SensorID");
        const JSON* sensorType = JSONGetObjectItem(jsonSensor, "SensorType");
        const JSON* sensorIsVirtual = JSONGetObjectItem(jsonSensor, "SensorIsVirtual");

        if (sensorID == NULL || sensorType == NULL || sensorIsVirtual == NULL)
        {
            log_error("sensor data incomplete");
            return NULL;
        }

        if(JSONIsTrue(sensorIsVirtual))
        {
            sensors[currentIndex].isVirtual = 1;
        }
        else if(JSONIsFalse(sensorIsVirtual))
        {
            sensors[currentIndex].isVirtual = 0;
        }
        else 
        {
            return NULL;
        }

        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        strcpy(sensors[currentIndex].sensorID, sensorID->valuestring);

        sensors[currentIndex].pinMapping = currentIndex;
        sensors[currentIndex].type = parseSensorType(sensorType->valuestring);

        if(sensors[currentIndex].type == SensorTypeUnknown)
        {
            log_error("unknown sensor type: %s", sensorType->valuestring);
            return NULL;
        }

        sensors[currentIndex].value = calloc(getValueSizeOfSensorType(sensors[currentIndex].type), 1);
        currentIndex++;
    }
    JSONDelete(jsonSensors);
    return sensors;
}

Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount, unsigned int sensorCount)
{
    Actuator* actuators;
    JSON* jsonActuators = JSONParseWithLength(str, length);
    if (jsonActuators == NULL)
    {
        log_error("parsing of actuator data json failed");
        return NULL;
    }
    *actuatorCount = JSONGetArraySize(jsonActuators);
    actuators = malloc(sizeof(*actuators)*(*actuatorCount));
    if (actuators == NULL)
    {
        log_error("malloc error: %s", strerror(errno));
        return NULL;
    }
    const JSON* jsonActuator = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonActuator, jsonActuators)
    {
        const JSON* actuatorID = JSONGetObjectItem(jsonActuator, "ActuatorID");
        const JSON* actuatorType = JSONGetObjectItem(jsonActuator, "ActuatorType");
        const JSON* actuatorStopValue = JSONGetObjectItem(jsonActuator, "ActuatorStopValue");

        if (actuatorID == NULL || actuatorType == NULL || actuatorStopValue == NULL)
        {
            log_error("actuator data incomplete!");
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        strcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring);

        actuators[currentIndex].pinMapping = sensorCount + currentIndex;
        actuators[currentIndex].type = parseActuatorType(actuatorType->valuestring);

        if(actuators[currentIndex].type == ActuatorTypeUnknown)
        {
            log_error("unknown sensor type: %s", actuatorType->valuestring);
            return NULL;
        }

        printf("TEST");
        printf("VALUE:%d\n", actuatorStopValue->valueint);

        actuators[currentIndex].value = unbeautifyActuatorValue(actuatorStopValue, actuators[currentIndex].type);
        actuators[currentIndex].stopValue = unbeautifyActuatorValue(actuatorStopValue, actuators[currentIndex].type);
        currentIndex++;
    }
    JSONDelete(jsonActuators);
    return actuators;
}

void printSensorData(Sensor sensor)
{
    printf("SensorID:               %s\n", sensor.sensorID);
    printf("SensorType:             %s\n", sensorTypeToString(sensor.type));
    printf("SensorValue:            [");
    for (int i = 0; i < getValueSizeOfSensorType(sensor.type); i++)
    {
        if (!(i == getValueSizeOfSensorType(sensor.type)-1)) 
        {
            printf("%d, ", sensor.value[i]);
        }
        else
        {
            printf("%d]\n", sensor.value[i]);
        }
    }
    if (sensor.isVirtual)
    {
        printf("SensorIsVirtual:        %s", "true\n");
    }
    else
    {
        printf("SensorIsVirtual:        %s", "false\n");
    }
    printf("\n");
}

void printActuatorData(Actuator actuator)
{
    printf("ActuatorID:             %s\n", actuator.actuatorID);
    printf("ActuatorType:           %s\n", actuatorTypeToString(actuator.type));
    printf("StopValue:              [");
    for (int i = 0; i < getValueSizeOfActuatorType(actuator.type); i++)
    {
        if (!(i == getValueSizeOfActuatorType(actuator.type)-1)) 
        {
            printf("%d, ", actuator.value[i]);
        }
        else
        {
            printf("%d]\n", actuator.value[i]);
        }
    }
    printf("ActuatorValue:          [");
    for (int i = 0; i < getValueSizeOfActuatorType(actuator.type); i++)
    {
        if (!(i == getValueSizeOfActuatorType(actuator.type)-1)) 
        {
            printf("%d, ", actuator.stopValue[i]);
        }
        else
        {
            printf("%d]\n", actuator.stopValue[i]);
        }
    }
    printf("\n");
}

void destroySensors(Sensor* sensors, unsigned int sensorCount)
{
    for (int i = 0; i < sensorCount; i++)
    {
        free(sensors[i].sensorID);
        free(sensors[i].value);
    }
    free(sensors);
}

void destroyActuators(Actuator* actuators, unsigned int actuatorCount)
{
    for (int i = 0; i < actuatorCount; i++)
    {
        free(actuators[i].actuatorID);
        free(actuators[i].value);
        free(actuators[i].stopValue);
    }
    free(actuators);
}

Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount)
{
    Sensor* current = sensors;
    for (int i = 0; i < sensorcount; i++)
    {
        if (!strcmp(current->sensorID, id))
        {
            return current;
        }
        current++;
    }
    return NULL;
}

Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount)
{
    Actuator* current = actuators;
    for (int i = 0; i < actuatorcount; i++)
    {
        if (!strcmp(current->actuatorID, id))
        {
            return current;
        }
        current++;
    }
    return NULL;
}

SensorDataPacket* parseSensorDataPackets(char* str, int length, unsigned int* packetCount)
{
    SensorDataPacket* sensors;
    JSON* jsonSensors = JSONParseWithLength(str, length);
    if (jsonSensors == NULL)
    {
        return NULL;
    }
    *packetCount = JSONGetArraySize(jsonSensors);
    sensors = malloc(sizeof(*sensors)*(*packetCount));
    if (sensors == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensor = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "ActuatorID");
        const JSON* sensorType = JSONGetObjectItem(jsonSensor, "ActuatorType");
        const JSON* sensorValue = JSONGetObjectItem(jsonSensor, "ActuatorValue");
        if (sensorID == NULL || sensorType == NULL || sensorValue == NULL)
        {
            log_error("sensor data packet incomplete!");
            return NULL;
        }
        
        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        memcpy(sensors[currentIndex].sensorID, sensorID->valuestring, strlen(sensorID->valuestring)+1);
        
        sensors[currentIndex].sensorType = parseSensorType(sensorType->valuestring);
        
        sensors[currentIndex].value = unbeautifySensorValue(sensorValue, sensors[currentIndex].sensorType);
        currentIndex++;
    }
    JSONDelete(jsonSensors);
    return sensors;
}

ActuatorDataPacket* parseActuatorDataPackets(char* str, int length, unsigned int* packetCount)
{
    ActuatorDataPacket* actuators;
    JSON* jsonActuators = JSONParseWithLength(str, length);
    if (jsonActuators == NULL)
    {
        return NULL;
    }
    *packetCount = JSONGetArraySize(jsonActuators);
    actuators = malloc(sizeof(*actuators)*(*packetCount));
    if (actuators == NULL)
    {
        return NULL;
    }
    const JSON* jsonActuator = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonActuator, jsonActuators)
    {
        const JSON* actuatorID = JSONGetObjectItem(jsonActuator, "ActuatorID");
        const JSON* actuatorType = JSONGetObjectItem(jsonActuator, "ActuatorType");
        const JSON* actuatorValue = JSONGetObjectItem(jsonActuator, "ActuatorValue");
        if (actuatorID == NULL || actuatorType == NULL || actuatorValue == NULL)
        {
            log_error("actuator data packet incomplete!");
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        memcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring, strlen(actuatorID->valuestring)+1);
        
        actuators[currentIndex].actuatorType = parseActuatorType(actuatorType->valuestring);
        
        actuators[currentIndex].value = unbeautifyActuatorValue(actuatorValue, actuators[currentIndex].actuatorType);
        currentIndex++;
    }
    JSONDelete(jsonActuators);
    return actuators;
}

JSON* SensorDataPacketToJSON(SensorDataPacket packet)
{
    JSON* dataPacket = beautifySensorValue(packet.value, packet.sensorType);
    JSONAddStringToObject(dataPacket, "SensorID", packet.sensorID);
    JSONAddStringToObject(dataPacket, "SensorType", sensorTypeToString(packet.sensorType));
    return dataPacket;
}

JSON* ActuatorDataPacketToJSON(ActuatorDataPacket packet)
{
    JSON* dataPacket = beautifyActuatorValue(packet.value, packet.actuatorType);
    JSONAddStringToObject(dataPacket, "ActuatorID", packet.actuatorID);
    JSONAddStringToObject(dataPacket, "ActuatorType", actuatorTypeToString(packet.actuatorType));
    return dataPacket;
}