#include "SensorsActuators.h"

#ifdef EXTENDED_SENSORS_ACTUATORS

Sensor* parseSensors(char* str, int length, unsigned int* sensorCount)
{
    Sensor* sensors;
    JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensors = JSONGetObjectItem(json, "Sensors");
    if (jsonSensors == NULL)
    {
        return NULL;
    }
    *sensorCount = JSONGetArraySize(jsonSensors);
    sensors = malloc(sizeof(*sensors)*(*sensorCount));
    if (sensors == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensor = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "SensorID");
        const JSON* sensorType = JSONGetObjectItem(jsonSensor, "SensorType");
        const JSON* sensorCommand = JSONGetObjectItem(jsonSensor, "SensorCommand");
        const JSON* sensorMathExpression = JSONGetObjectItem(jsonSensor, "SensorMathExpression");
        const JSON* sensorIsVirtual = JSONGetObjectItem(jsonSensor, "SensorIsVirtual");
        const JSON* sensorValueBitWidth = JSONGetObjectItem(jsonSensor, "SensorValueBitWidth");
        if (sensorID == NULL || sensorType == NULL || sensorCommand == NULL || 
            sensorIsVirtual == NULL || sensorValueBitWidth == NULL || sensorMathExpression == NULL)
        {
            printf("sth could not be found!\n");
            return NULL;
        }
        
        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        strcpy(sensors[currentIndex].sensorID, sensorID->valuestring);
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
            return NULL;
        }

        sensors[currentIndex].command.answerLength = JSONGetObjectItem(sensorCommand, "AnswerLength")->valueint;
        sensors[currentIndex].command.dataLength = JSONGetObjectItem(sensorCommand, "DataLength")->valueint;
        sensors[currentIndex].command.commandLength = JSONGetArraySize(JSONGetObjectItem(sensorCommand, "Command"));
        sensors[currentIndex].command.command = malloc(JSONGetArraySize(JSONGetObjectItem(sensorCommand, "Command")));
        const JSON* commandByte = NULL;
        int currentCommandByteIndex = 0;
        JSONArrayForEach(commandByte, JSONGetObjectItem(sensorCommand, "Command"))
        {
            sensors[currentIndex].command.command[currentCommandByteIndex++] = commandByte->valueint;
        }
        sensors[currentIndex].value = 0;
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
        sensors[currentIndex].valueBitWidth = sensorValueBitWidth->valueint;
        currentIndex++;
    }
    JSONDelete(json);
    return sensors;
}

Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount)
{
    Actuator* actuators;
    JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonActuators = JSONGetObjectItem(json, "Actuators");
    if (jsonActuators == NULL)
    {
        return NULL;
    }
    *actuatorCount = JSONGetArraySize(jsonActuators);
    actuators = malloc(sizeof(*actuators)*(*actuatorCount));
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
        const JSON* actuatorCommand = JSONGetObjectItem(jsonActuator, "ActuatorCommand");
        const JSON* actuatorMathExpression = JSONGetObjectItem(jsonActuator, "ActuatorMathExpression");
        const JSON* actuatorValueBitWidth = JSONGetObjectItem(jsonActuator, "ActuatorValueBitWidth");
        if (actuatorID == NULL || actuatorType == NULL || actuatorCommand == NULL || 
            actuatorValueBitWidth == NULL || actuatorMathExpression == NULL)
        {
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        strcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring);
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
            return NULL;
        }

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
        actuators[currentIndex].command.commandLength = JSONGetArraySize(JSONGetObjectItem(actuatorCommand, "Command"));
        actuators[currentIndex].command.command = malloc(JSONGetArraySize(JSONGetObjectItem(actuatorCommand, "Command")));
        actuators[currentIndex].value = (long long)JSONGetNumberValue(JSONGetObjectItem(actuatorCommand, "StopValue"));
        actuators[currentIndex].valueDouble = JSONGetNumberValue(JSONGetObjectItem(actuatorCommand, "StopValue"));
        actuators[currentIndex].stopData = ActuatorValueToSPIData(&actuators[currentIndex]);

        const JSON* byte = NULL;
        int byteIndex = 0;
        JSONArrayForEach(byte, JSONGetObjectItem(actuatorCommand, "Command"))
        {
            actuators[currentIndex].command.command[byteIndex++] = byte->valueint;
        }
        byteIndex = 0;

        actuators[currentIndex].valueBitWidth = actuatorValueBitWidth->valueint;
        currentIndex++;
    }
    JSONDelete(json);
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
    printf("SensorCommand:          ");
    for (int i; i < sensor.command.commandLength; i++)
    {
        printf("%x ",sensor.command.command[i]);
    }
    printf("\n");
    printf("SensorValue:            %lld\n", sensor.value);
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
    printf("SensorValueBitWidth:    %d\n", sensor.valueBitWidth);
    printf("\n");
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
    printf("ActuatorCommand:        ");
    for (int i; i < actuator.command.commandLength; i++)
    {
        printf("%x ",actuator.command.command[i]);
    }
    printf("\n");
    printf("StopData:               ");
    for (int i; i < actuator.command.dataLength; i++)
    {
        printf("%x ",actuator.stopData[i]);
    }
    printf("\n");
    printf("ActuatorValue:          %lld\n", actuator.value);
    if (actuator.mathExpr != NULL)
    {
        printf("ActuatorMathExpression: %s, result = %lld\n", "true", (long long)evaluateMathematicalExpression(actuator.mathExpr));
    }
    else 
    {
        printf("ActuatorMathExpression: %s\n", "false");
    }
    printf("ActuatorValueBitWidth:  %d\n", actuator.valueBitWidth);
    printf("\n");
}

void destroySensors(Sensor* sensors, unsigned int sensorCount)
{
    for (int i = 0; i < sensorCount; i++)
    {
        if(sensors[i].mathExpr != NULL)
            destroyMathematicalExpression(sensors[i].mathExpr);
        free(sensors[i].command.command);
        free(sensors[i].sensorID);
    }
    free(sensors);
}

void destroyActuators(Actuator* actuators, unsigned int actuatorCount)
{
    for (int i = 0; i < actuatorCount; i++)
    {
        if(actuators[i].mathExpr != NULL)
            destroyMathematicalExpression(actuators[i].mathExpr);
        free(actuators[i].stopData);
        free(actuators[i].command.command);
        free(actuators[i].actuatorID);
    }
    free(actuators);
}

// TODO: look if it needs a revamp
char* ActuatorValueToSPIData(Actuator* actuator)
{
    char* result = malloc(sizeof(long long));
    long long value = actuator->value;
    if (actuator->mathExpr != NULL)
    {
        value = evaluateMathematicalExpression(actuator->mathExpr);
    }
    for (int i = 0; i < sizeof(long long); i++)
    {
        result[i] = (value >> ((sizeof(long long) - (i+1)) * 8));
    }
    return result;
}

#else

Sensor* parseSensors(char* str, int length, unsigned int* sensorCount)
{
    Sensor* sensors;
    JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensors = JSONGetObjectItem(json, "Sensors");
    if (jsonSensors == NULL)
    {
        return NULL;
    }
    *sensorCount = JSONGetArraySize(jsonSensors);
    sensors = malloc(sizeof(*sensors)*(*sensorCount));
    if (sensors == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensor = NULL;
    int currentIndex = 0;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "SensorID");
        const JSON* sensorType = JSONGetObjectItem(jsonSensor, "SensorType");
        const JSON* sensorValueBitWidth = JSONGetObjectItem(jsonSensor, "SensorValueBitWidth");
        if (sensorID == NULL || sensorType == NULL || sensorValueBitWidth == NULL)
        {
            printf("sth could not be found!\n");
            return NULL;
        }
        
        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        memcpy(sensors[currentIndex].sensorID, sensorID->valuestring, strlen(sensorID->valuestring)+1);
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
            return NULL;
        }

        sensors[currentIndex].value = 0;
        sensors[currentIndex].valueBitWidth = sensorValueBitWidth->valueint;
        currentIndex++;
    }
    JSONDelete(json);
    return sensors;
}

Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount)
{
    Actuator* actuators;
    JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonActuators = JSONGetObjectItem(json, "Actuators");
    if (jsonActuators == NULL)
    {
        return NULL;
    }
    *actuatorCount = JSONGetArraySize(jsonActuators);
    actuators = malloc(sizeof(*actuators)*(*actuatorCount));
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
        const JSON* actuatorCommand = JSONGetObjectItem(jsonActuator, "ActuatorCommand");
        const JSON* actuatorValueBitWidth = JSONGetObjectItem(jsonActuator, "ActuatorValueBitWidth");
        if (actuatorID == NULL || actuatorType == NULL || actuatorCommand == NULL || actuatorValueBitWidth == NULL)
        {
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        memcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring, strlen(actuatorID->valuestring)+1);
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
            return NULL;
        }

        actuators[currentIndex].value = (long long)JSONGetNumberValue(JSONGetObjectItem(actuatorCommand, "StopValue"));
        actuators[currentIndex].stopValue = (long long)JSONGetNumberValue(JSONGetObjectItem(actuatorCommand, "StopValue"));
        actuators[currentIndex].valueBitWidth = actuatorValueBitWidth->valueint;
        currentIndex++;
    }
    JSONDelete(json);
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
    printf("SensorValueBitWidth:    %d\n", sensor.valueBitWidth);
    printf("\n");
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
    printf("StopValue:              %lld\n", actuator.stopValue);
    printf("ActuatorValue:          %lld\n", actuator.value);
    printf("ActuatorValueBitWidth:  %d\n", actuator.valueBitWidth);
    printf("\n");
}

void destroySensors(Sensor* sensors, unsigned int sensorCount)
{
    for (int i = 0; i < sensorCount; i++)
    {
        free(sensors[i].sensorID);
    }
    free(sensors);
}

void destroyActuators(Actuator* actuators, unsigned int actuatorCount)
{
    for (int i = 0; i < actuatorCount; i++)
    {
        free(actuators[i].actuatorID);
    }
    free(actuators);
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

long long SensorSPIDataToValue(Sensor* sensor)
{

}