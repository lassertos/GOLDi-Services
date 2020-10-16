#include "SensorsActuators.h"

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
    sensors = malloc(sizeof(Sensor)*JSONGetArraySize(jsonSensors));
    if (sensors == NULL)
    {
        return NULL;
    }
    const JSON* jsonSensor = NULL;
    JSONArrayForEach(jsonSensor, jsonSensors)
    {
        const JSON* sensorID = JSONGetObjectItem(jsonSensor, "SensorID");
        const JSON* sensorType = JSONGetObjectItem(jsonSensor, "SensorType");
        const JSON* sensorCommand = JSONGetObjectItem(jsonSensor, "SensorCommand");
        const JSON* sensorMathExpression = JSONGetObjectItem(jsonSensor, "SensorMathExpression");
        const JSON* sensorBooleanExpression = JSONGetObjectItem(jsonSensor, "SensorBooleanExpression");
        const JSON* sensorIsVirtual = JSONGetObjectItem(jsonSensor, "SensorIsVirtual");
        if (sensorID == NULL || sensorType == NULL || sensorCommand == NULL || 
            sensorMathExpression == NULL || sensorBooleanExpression == NULL || sensorIsVirtual == NULL)
        {
            return NULL;
        }

    }
}

Actuator* parseActuators(char* str, int length)
{
    
}

Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount)
{
    Sensor* current = sensors;
    for (int i = 0; i < sensorcount; i++)
    {
        if (!strcmp(current->SensorID, id))
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
        if (!strcmp(current->ActuatorID, id))
        {
            return current;
        }
        current++;
    }
    return NULL;
}