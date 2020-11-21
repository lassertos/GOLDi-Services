#include "SensorsActuators.h"
#include "../logging/log.h"

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

        if (sensorID == NULL || sensorType == NULL)
        {
            log_error("sensor data incomplete");
            return NULL;
        }

        #ifdef EXTENDED_SENSORS_ACTUATORS

        const JSON* sensorCommand = JSONGetObjectItem(jsonSensor, "SensorCommand");
        const JSON* sensorMathExpression = JSONGetObjectItem(jsonSensor, "SensorMathExpression");
        const JSON* sensorIsVirtual = JSONGetObjectItem(jsonSensor, "SensorIsVirtual");

        if (sensorCommand == NULL || sensorIsVirtual == NULL || sensorMathExpression == NULL)
        {
            log_error("sensor data incomplete");
            return NULL;
        }

        sensors[currentIndex].command.answerLength = JSONGetObjectItem(sensorCommand, "AnswerLength")->valueint;
        sensors[currentIndex].command.dataLength = JSONGetObjectItem(sensorCommand, "DataLength")->valueint;
        const JSON* commandByte = NULL;
        int currentCommandByteIndex = 0;
        sensors[currentIndex].command.command = JSONGetObjectItem(sensorCommand, "Command")->valueint;
        sensors[currentIndex].valueDouble = 0;

        if (JSONIsNull(sensorMathExpression))
        {
            sensors[currentIndex].mathExpr = NULL;
        }
        else 
        {
            Variable variables[] = {{sensors[currentIndex].sensorID, &sensors[currentIndex].valueDouble}};
            sensors[currentIndex].mathExpr = createMathematicalExpression(sensorMathExpression->valuestring, variables, 1, NULL);
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
        
        #endif

        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        strcpy(sensors[currentIndex].sensorID, sensorID->valuestring);

        sensors[currentIndex].pinMapping = currentIndex;

        if(!strcmp(sensorType->valuestring,"binary"))
        {
            sensors[currentIndex].type = SensorTypeBinary;
        }
        else if(!strcmp(sensorType->valuestring,"analog"))
        {
            sensors[currentIndex].type = SensorTypeAnalog;
        }
        else if(!strcmp(sensorType->valuestring,"protocol"))
        {
            sensors[currentIndex].type = SensorTypeProtocol;
        }
        else
        {
            log_error("unknown sensor type: %s", sensorType->valuestring);
            return NULL;
        }

        sensors[currentIndex].value = 0;
        currentIndex++;
    }
    JSONDelete(jsonSensors);
    return sensors;
}

Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount)
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

        if (actuatorID == NULL || actuatorType == NULL)
        {
            log_error("actuator data incomplete!");
            return NULL;
        }

        #ifdef EXTENDED_SENSORS_ACTUATORS

        const JSON* actuatorCommand = JSONGetObjectItem(jsonActuator, "ActuatorCommand");
        const JSON* actuatorMathExpression = JSONGetObjectItem(jsonActuator, "ActuatorMathExpression");
        const JSON* actuatorStopValue = JSONGetObjectItem(jsonActuator, "ActuatorStopValue");
        if (actuatorCommand == NULL || actuatorMathExpression == NULL || actuatorStopValue == NULL)
        {
            log_error("actuator data incomplete!");
            return NULL;
        }

        actuators[currentIndex].valueDouble = JSONGetNumberValue(actuatorStopValue);

        if (JSONIsNull(actuatorMathExpression))
        {
            actuators[currentIndex].mathExpr = NULL;
        }
        else 
        {
            Variable variables[] = {{actuators[currentIndex].actuatorID, &actuators[currentIndex].valueDouble}};
            actuators[currentIndex].mathExpr = createMathematicalExpression(actuatorMathExpression->valuestring, variables, 1, NULL);
        }
        
        actuators[currentIndex].command.answerLength = JSONGetObjectItem(actuatorCommand, "AnswerLength")->valueint;
        actuators[currentIndex].command.dataLength = JSONGetObjectItem(actuatorCommand, "DataLength")->valueint;
        actuators[currentIndex].command.command = JSONGetObjectItem(actuatorCommand, "Command")->valueint;
        actuators[currentIndex].stopData = ActuatorValueToSPIData(&actuators[currentIndex]);

        #endif
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        strcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring);

        actuators[currentIndex].pinMapping = currentIndex;

        if(!strcmp(actuatorType->valuestring,"binary"))
        {
            actuators[currentIndex].type = ActuatorTypeBinary;
        }
        else if(!strcmp(actuatorType->valuestring,"analog"))
        {
            actuators[currentIndex].type = ActuatorTypeAnalog;
        }
        else if(!strcmp(actuatorType->valuestring,"protocol"))
        {
            actuators[currentIndex].type = ActuatorTypeProtocol;
        }
        else
        {
            log_error("unknown actuator type: %s", actuatorType->valuestring);
            return NULL;
        }

        actuators[currentIndex].value = (long long)JSONGetNumberValue(JSONGetObjectItem(jsonActuator, "ActuatorStopValue"));
        #ifndef EXTENDED_SENSORS_ACTUATORS
        actuators[currentIndex].stopValue = (long long)JSONGetNumberValue(JSONGetObjectItem(jsonActuator, "ActuatorStopValue"));
        #endif
        currentIndex++;
    }
    JSONDelete(jsonActuators);
    return actuators;
}

void printSensorData(Sensor sensor)
{
    printf("SensorID:               %s\n", sensor.sensorID);
    switch (sensor.type) 
    {
        case SensorTypeBinary:
            printf("SensorType:             %s", "binary\n");
            break;
        case SensorTypeAnalog:
            printf("SensorType:             %s", "analog\n");
            break;
        case SensorTypeProtocol:
            printf("SensorType:             %s", "protocol\n");
    }
    printf("SensorValue:            %lld\n", sensor.value);

    #ifdef EXTENDED_SENSORS_ACTUATORS

    printf("SensorCommand:          %x\n",sensor.command.command);
    printf("SensorValueDouble:      %f\n", sensor.valueDouble);
    if (sensor.mathExpr != NULL)
    {
        printf("SensorMathExpression:   %s, result = %lld\n", "true", (long long)evaluateMathematicalExpression(sensor.mathExpr));
    }
    else 
    {
        printf("SensorMathExpression:   %s\n", "false");
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

    #endif
}

void printActuatorData(Actuator actuator)
{
    printf("ActuatorID:             %s\n", actuator.actuatorID);
    switch (actuator.type) 
    {
        case ActuatorTypeBinary:
            printf("ActuatorType:           %s", "binary\n");
            break;
        case ActuatorTypeAnalog:
            printf("ActuatorType:           %s", "analog\n");
            break;
        case ActuatorTypeProtocol:
            printf("ActuatorType:           %s", "protocol\n");
    }
    #ifndef EXTENDED_SENSORS_ACTUATORS
    printf("StopValue:              %lld\n", actuator.stopValue);
    #endif
    printf("ActuatorValue:          %lld\n", actuator.value);
    #ifdef EXTENDED_SENSORS_ACTUATORS

    printf("ActuatorCommand:        %x\n",actuator.command.command);
    printf("StopData:               ");
    for (int i; i < actuator.command.dataLength; i++)
    {
        printf("%x ",actuator.stopData[i]);
    }
    printf("\n");
    printf("ActuatorValueDouble:    %f\n", actuator.valueDouble);
    if (actuator.mathExpr != NULL)
    {
        printf("ActuatorMathExpression: %s, result = %lld\n", "true", (long long)evaluateMathematicalExpression(actuator.mathExpr));
    }
    else 
    {
        printf("ActuatorMathExpression: %s\n", "false");
    }
    printf("\n");

    #endif
}

void destroySensors(Sensor* sensors, unsigned int sensorCount)
{
    for (int i = 0; i < sensorCount; i++)
    {
        #ifdef EXTENDED_SENSORS_ACTUATORS
        if(sensors[i].mathExpr != NULL)
            destroyMathematicalExpression(sensors[i].mathExpr);
        #endif
        free(sensors[i].sensorID);
    }
    free(sensors);
}

void destroyActuators(Actuator* actuators, unsigned int actuatorCount)
{
    for (int i = 0; i < actuatorCount; i++)
    {
        #ifdef EXTENDED_SENSORS_ACTUATORS
        if(actuators[i].mathExpr != NULL)
            destroyMathematicalExpression(actuators[i].mathExpr);
        free(actuators[i].stopData);
        #endif
        free(actuators[i].actuatorID);
    }
    free(actuators);
}

#ifdef EXTENDED_SENSORS_ACTUATORS

// TODO: look if it needs a revamp 
char* ActuatorValueToSPIData(Actuator* actuator)
{
    char* result = malloc(actuator->command.dataLength);
    switch(actuator->type)
    {
        case ActuatorTypeBinary:
        {
            if (!actuator->value)
            {
                result[0] = 0;
            }
            else
            {
                result[0] = 1;
            }
        }
        
        default:
        {
            return NULL;
        }
    }
    // old implementation can be deleted if not needed anymore
    /* char* result = malloc(sizeof(long long));
    long long value = actuator->value;
    if (actuator->mathExpr != NULL)
    {
        value = evaluateMathematicalExpression(actuator->mathExpr);
    }
    for (int i = 0; i < sizeof(long long); i++)
    {
        result[i] = (value >> ((sizeof(long long) - (i+1)) * 8));
    } */
    return result;
}

void SPIAnswerToSensorValue(Sensor* sensor, spiAnswer answer)
{
    long long value = 0;

    for (int i = 0; i < answer.length; i++)
    {
        if (value != 0)
        {
            value = value << 8;
        }
        value += answer.content[i];
    }

    sensor->value = value;
    sensor->valueDouble = (double)value;

    if (sensor->mathExpr != NULL)
    {
        sensor->valueDouble = evaluateMathematicalExpression(sensor->mathExpr);
        sensor->value = (long long)sensor->valueDouble;
    }
}

#endif

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
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "SensorID");
        if (sensorID == NULL)
        {
            log_error("sensor data packet incomplete!");
            return NULL;
        }
        
        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        memcpy(sensors[currentIndex].sensorID, sensorID->valuestring, strlen(sensorID->valuestring)+1);

        sensors[currentIndex].value = JSONGetObjectItem(jsonSensors, "SensorValue")->valueint;
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
        if (actuatorID == NULL)
        {
            log_error("actuator data packet incomplete!");
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        memcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring, strlen(actuatorID->valuestring)+1);

        actuators[currentIndex].value = JSONGetObjectItem(jsonActuators, "ActuatorValue")->valueint;
        currentIndex++;
    }
    JSONDelete(jsonActuators);
    return actuators;
}

JSON* SensorDataPacketToJSON(SensorDataPacket packet)
{
    JSON* dataPacket = JSONCreateObject();
    JSONAddStringToObject(dataPacket, "SensorID", packet.sensorID);
    JSONAddNumberToObject(dataPacket, "SensorValue", packet.value);
    return dataPacket;
}

JSON* ActuatorDataPacketToJSON(ActuatorDataPacket packet)
{
    JSON* dataPacket = JSONCreateObject();
    JSONAddStringToObject(dataPacket, "ActuatorID", packet.actuatorID);
    JSONAddNumberToObject(dataPacket, "ActuatorValue", packet.value);
    return dataPacket;
}