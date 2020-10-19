#ifndef SENSORSACTUATORS_H
#define SENSORSACTUATORS_H

#define PHYSICAL_SYSTEM

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

#ifdef PHYSICAL_SYSTEM
typedef struct
{
    char*                   sensorID;
    SensorType              type;
    spiCommand              command;
    long long               value;
    MathematicalExpression* mathExpr;
    unsigned int            isVirtual;
    unsigned int            valueBitWidth;
} Sensor;

typedef struct
{
    char*                   actuatorID;
    ActuatorType            type;
    spiCommand              command;
    char*                   stopData;
    long long               value;
    MathematicalExpression* mathExpr;
    spiCommand              stopCommand;
    unsigned int            valueBitWidth;
} Actuator;

#else

typedef struct
{
    char*                   sensorID;
    SensorType              type;
    long long               value;
    unsigned int            valueBitWidth;
} Sensor;

typedef struct
{
    char*                   actuatorID;
    ActuatorType            type;
    long long               value;
    unsigned int            valueBitWidth;
} Actuator;

#endif


Sensor* parseSensors(char* str, int length);
Actuator* parseActuators(char* str, int length);
Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount);
Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount);
long long calculateSensorValue(Sensor* sensor);
char* retrieveActuatorValue(Actuator* actuator);
void printSensorData(Sensor sensor);
void printActuatorData(Actuator actuator);

#endif