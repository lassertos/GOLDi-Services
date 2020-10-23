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

#ifdef EXTENDED_SENSORS_ACTUATORS
typedef struct
{
    char*                   sensorID;
    SensorType              type;
    spiCommand              command;
    long long               value;
    double                  valueDouble;
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
    double                  valueDouble;
    MathematicalExpression* mathExpr;
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
    long long               stopValue;
    unsigned int            valueBitWidth;
} Actuator;

#endif


Sensor* parseSensors(char* str, int length, unsigned int* sensorCount);
Actuator* parseActuators(char* str, int length, unsigned int* actuatorCount);
Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount);
Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount);
long long SensorSPIDataToValue(Sensor* sensor);
char* ActuatorValueToSPIData(Actuator* actuator);
void destroySensors(Sensor* sensors, unsigned int sensorCount);
void destroyActuators(Actuator* actuators, unsigned int actuatorCount);
void printSensorData(Sensor sensor);
void printActuatorData(Actuator actuator);

#endif