#ifndef SENSORSACTUATORS_H
#define SENSORSACTUATORS_H

#include "ExpressionParsers.h"
#include "spi.h"
#include "json.h"
#include <stdio.h>

typedef enum 
{
    SensorTypeBinary
} SensorType;

typedef enum 
{
    ActuatorTypeBinary
} ActuatorType;

typedef struct Sensor_s
{
    char*                   sensorID;
    SensorType              type;
    spiCommand              command;
    char*                   value;
    BooleanExpression*      boolExpr;
    MathematicalExpression* mathExpr;
    unsigned int            isVirtual;
} Sensor;

typedef struct Actuator_s
{
    char*                   actuatorID;
    ActuatorType            type;
    spiCommand              command;
    char*                   value;
    BooleanExpression*      boolExpr;
    MathematicalExpression* mathExpr;
    spiCommand              stopCommand;
} Actuator;

Sensor* parseSensors(char* str, int length);
Actuator* parseActuators(char* str, int length);
Sensor* getSensorWithID(Sensor* sensors, char* id, int sensorcount);
Actuator* getActuatorWithID(Actuator* actuators, char* id, int actuatorcount);

#endif