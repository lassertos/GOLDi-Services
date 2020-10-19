#include "SensorsActuators.h"

#ifdef PHYSICAL_SYSTEM

Sensor* parseSensors(char* str, int length)
{
    Sensor* sensors;
    const JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensors = JSONGetObjectItem(json, "Sensors");
    if (jsonSensors == NULL)
    {
        return NULL;
    }
    sensors = malloc(sizeof(*sensors)*JSONGetArraySize(jsonSensors));
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
        memcpy(sensors[currentIndex].sensorID, sensorID->valuestring, strlen(sensorID->valuestring)+1);
        if(!strcmp(sensorType->valuestring,"binary"))
        {
            sensors[currentIndex].type = SensorTypeBinary;
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

        if (JSONIsNull(sensorMathExpression))
        {
            sensors[currentIndex].mathExpr = NULL;
        }
        else 
        {
            Variable variables[] = {{sensors[currentIndex].sensorID, &sensors[currentIndex].value}};
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
    return sensors;
}

Actuator* parseActuators(char* str, int length)
{
    Actuator* actuators;
    const JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonActuators = JSONGetObjectItem(json, "Actuators");
    if (jsonActuators == NULL)
    {
        return NULL;
    }
    actuators = malloc(sizeof(*actuators)*JSONGetArraySize(jsonActuators));
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
        memcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring, strlen(actuatorID->valuestring)+1);
        if(!strcmp(actuatorType->valuestring,"binary"))
        {
            actuators[currentIndex].type = ActuatorTypeBinary;
        }
        else
        {
            return NULL;
        }

        actuators[currentIndex].command.answerLength = JSONGetObjectItem(actuatorCommand, "AnswerLength")->valueint;
        actuators[currentIndex].command.dataLength = JSONGetObjectItem(actuatorCommand, "DataLength")->valueint;
        actuators[currentIndex].command.commandLength = JSONGetArraySize(JSONGetObjectItem(actuatorCommand, "Command"));
        actuators[currentIndex].command.command = malloc(JSONGetArraySize(JSONGetObjectItem(actuatorCommand, "Command")));
        actuators[currentIndex].stopData = malloc(JSONGetArraySize(JSONGetObjectItem(actuatorCommand, "StopData")));

        const JSON* byte = NULL;
        int byteIndex = 0;
        JSONArrayForEach(byte, JSONGetObjectItem(actuatorCommand, "Command"))
        {
            actuators[currentIndex].command.command[byteIndex++] = byte->valueint;
        }
        byteIndex = 0;
        JSONArrayForEach(byte, JSONGetObjectItem(actuatorCommand, "StopData"))
        {
            actuators[currentIndex].stopData[byteIndex++] = byte->valueint;
        }
        actuators[currentIndex].value = 0;

        if (JSONIsNull(actuatorMathExpression))
        {
            actuators[currentIndex].mathExpr = NULL;
        }
        else 
        {
            Variable variables[] = {{actuators[currentIndex].actuatorID, &actuators[currentIndex].value}};
            actuators[currentIndex].mathExpr = createMathematicalExpression(actuatorMathExpression->valuestring, variables, 1, NULL);
        }

        actuators[currentIndex].valueBitWidth = actuatorValueBitWidth->valueint;
        currentIndex++;
    }
    return actuators;
}

void printSensorData(Sensor sensor)
{
    printf("SensorID:               %s\n", sensor.sensorID);
    if(sensor.type == SensorTypeBinary)
        printf("SensorType:             %s", "binary\n");
    printf("SensorCommand:          ");
    for (int i; i < sensor.command.commandLength; i++)
    {
        printf("%x ",sensor.command.command[i]);
    }
    printf("\n");
    printf("SensorValue:            %lld\n", sensor.value);
    if (sensor.mathExpr != NULL)
        printf("SensorMathExpression:   %s, result = %lld\n", "true", (long long)evaluateMathematicalExpression(sensor.mathExpr));
    else 
        printf("SensorMathExpression:   %s\n", "false");
    if (sensor.isVirtual)
        printf("SensorIsVirtual:        %s", "true\n");
    else
        printf("SensorIsVirtual:        %s", "false\n");
    printf("SensorValueBitWidth:    %d\n", sensor.valueBitWidth);
}

void printActuatorData(Actuator actuator)
{
    printf("ActuatorID:               %s\n", actuator.actuatorID);
    if(actuator.type == ActuatorTypeBinary)
        printf("ActuatorType:             %s", "binary\n");
    printf("ActuatorCommand:          ");
    for (int i; i < actuator.command.commandLength; i++)
    {
        printf("%x ",actuator.command.command[i]);
    }
    printf("\n");
    printf("StopData:          ");
    for (int i; i < actuator.command.dataLength; i++)
    {
        printf("%x ",actuator.stopData[i]);
    }
    printf("\n");
    printf("ActuatorValue:            %lld\n", actuator.value);
    if (actuator.mathExpr != NULL)
        printf("ActuatorMathExpression:   %s, result = %lld\n", "true", (long long)evaluateMathematicalExpression(actuator.mathExpr));
    else 
        printf("ActuatorMathExpression:   %s\n", "false");
    printf("ActuatorValueBitWidth:    %d\n", actuator.valueBitWidth);
}

#else

Sensor* parseSensors(char* str, int length)
{
    Sensor* sensors;
    const JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensors = JSONGetObjectItem(json, "Sensors");
    if (jsonSensors == NULL)
    {
        return NULL;
    }
    sensors = malloc(sizeof(*sensors)*JSONGetArraySize(jsonSensors));
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
            return NULL;
        }
        
        sensors[currentIndex].sensorID = malloc(strlen(sensorID->valuestring)+1);
        memcpy(sensors[currentIndex].sensorID, sensorID->valuestring, strlen(sensorID->valuestring)+1);
        if(!strcmp(sensorType->valuestring,"binary"))
        {
            sensors[currentIndex].type = SensorTypeBinary;
        }
        else
        {
            return NULL;
        }
        sensors[currentIndex].value = 0;
        sensors[currentIndex].valueBitWidth = sensorValueBitWidth->valueint;
        currentIndex++;
    }
    return sensors;
}

Actuator* parseActuators(char* str, int length)
{
    Actuator* actuators;
    const JSON* json = JSONParseWithLength(str, length);
    if (json == NULL)
    {
        return NULL;
    }
    const JSON* jsonActuators = JSONGetObjectItem(json, "Actuators");
    if (jsonActuators == NULL)
    {
        return NULL;
    }
    actuators = malloc(sizeof(*actuators)*JSONGetArraySize(jsonActuators));
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
        const JSON* actuatorValueBitWidth = JSONGetObjectItem(jsonActuator, "ActuatorValueBitWidth");
        if (actuatorID == NULL || actuatorType == NULL || actuatorValueBitWidth == NULL)
        {
            return NULL;
        }
        
        actuators[currentIndex].actuatorID = malloc(strlen(actuatorID->valuestring)+1);
        memcpy(actuators[currentIndex].actuatorID, actuatorID->valuestring, strlen(actuatorID->valuestring)+1);
        if(!strcmp(actuatorType->valuestring,"binary"))
        {
            actuators[currentIndex].type = SensorTypeBinary;
        }
        else
        {
            return NULL;
        }
        actuators[currentIndex].value = 0;
        actuators[currentIndex].valueBitWidth = actuatorValueBitWidth->valueint;
        currentIndex++;
    }
    return actuators;
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