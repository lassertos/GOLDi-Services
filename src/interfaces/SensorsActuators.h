#ifndef SENSORSACTUATORS_H
#define SENSORSACTUATORS_H

#include "../parsers/json.h"
#include <stdio.h>

typedef enum 
{
    SensorTypeUnknown,
    SensorTypeBinary,
    SensorTypeAnalog,
    SensorTypeProtocol
} SensorType;

typedef enum 
{
    ActuatorTypeUnknown,
    ActuatorTypeBinary,
    ActuatorTypeAnalog,
    ActuatorTypeProtocol
} ActuatorType;

typedef struct 
{
    char* sensorID;
    SensorType sensorType;
    char* value;
} SensorDataPacket;

typedef struct 
{
    char* actuatorID;
    ActuatorType actuatorType;
    char* value;
} ActuatorDataPacket;

typedef struct
{
    char*           sensorID;
    SensorType      type;
    char*           value;
    unsigned char   pinMapping;
    unsigned int    isVirtual;
} Sensor;

typedef struct
{
    char*           actuatorID;
    ActuatorType    type;
    char*           value;
    unsigned char   pinMapping;
    char*           stopValue;
} Actuator;

Sensor* parseSensors(char* str, int length, unsigned int* sensorCount);
Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount, unsigned int sensorCount);

SensorDataPacket* parseSensorDataPackets(char* str, int length, unsigned int* packetCount);
ActuatorDataPacket* parseActuatorDataPackets(char* str, int length, unsigned int* packetCount);
JSON* SensorDataPacketToJSON(SensorDataPacket packet);
JSON* ActuatorDataPacketToJSON(ActuatorDataPacket packet);

Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount);
Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount);

unsigned int getValueSizeOfSensorType(SensorType sensorType);
unsigned int getValueSizeOfActuatorType(ActuatorType actuatorType);

char* unbeautifySensorValue(JSON* valueJSON, SensorType sensorType);
char* unbeautifyActuatorValue(JSON* valueJSON, ActuatorType actuatorType);

void destroySensors(Sensor* sensors, unsigned int sensorCount);
void destroyActuators(Actuator* actuators, unsigned int actuatorCount);

void printSensorData(Sensor sensor);
void printActuatorData(Actuator actuator);

#endif