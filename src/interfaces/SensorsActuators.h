#ifndef SENSORSACTUATORS_H
#define SENSORSACTUATORS_H

#include "../parsers/ExpressionParsers.h"
#include "spi.h"
#include "../parsers/json.h"
#include <stdio.h>

typedef enum 
{
    SensorTypeBinary,
    SensorTypeAnalog,
    SensorTypeProtocol
} SensorType;

typedef enum 
{
    ActuatorTypeBinary,
    ActuatorTypeAnalog,
    ActuatorTypeProtocol
} ActuatorType;

typedef struct 
{
    char* sensorID;
    long long value;
} SensorDataPacket;

typedef struct 
{
    char* actuatorID;
    long long value;
} ActuatorDataPacket;

typedef struct
{
    char*                   sensorID;
    SensorType              type;
    long long               value;
    unsigned char           pinMapping;
    #ifdef EXTENDED_SENSORS_ACTUATORS
    spiCommand              command;
    double                  valueDouble;
    MathematicalExpression* mathExpr;
    unsigned int            isVirtual;
    #endif
} Sensor;

typedef struct
{
    char*                   actuatorID;
    ActuatorType            type;
    long long               value;
    unsigned char           pinMapping;
    #ifdef EXTENDED_SENSORS_ACTUATORS
    spiCommand              command;
    char*                   stopData;
    double                  valueDouble;
    MathematicalExpression* mathExpr;
    #else
    long long               stopValue;
    #endif
} Actuator;


#ifdef EXTENDED_SENSORS_ACTUATORS
void SPIAnswerToSensorValue(Sensor* sensor, spiAnswer answer);
char* ActuatorValueToSPIData(Actuator* actuator);
#endif

Sensor* parseSensors(char* str, int length, unsigned int* sensorCount);
Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount);

SensorDataPacket* parseSensorDataPackets(char* str, int length, unsigned int* packetCount);
ActuatorDataPacket* parseActuatorDataPackets(char* str, int length, unsigned int* packetCount);
JSON* SensorDataPacketToJSON(SensorDataPacket packet);
JSON* ActuatorDataPacketToJSON(ActuatorDataPacket packet);

Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount);
Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount);

void destroySensors(Sensor* sensors, unsigned int sensorCount);
void destroyActuators(Actuator* actuators, unsigned int actuatorCount);

void printSensorData(Sensor sensor);
void printActuatorData(Actuator actuator);

#endif