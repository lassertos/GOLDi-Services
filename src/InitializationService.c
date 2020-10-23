#include "parsers/StateMachine.h"
#include "interfaces/SensorsActuators.h"
#include "utils/utils.h"

int main(int argc, char const *argv[])
{
    unsigned int sensorCount = 0;
    unsigned int actuatorCount = 0;
    unsigned int stateMachineCount = 0;
    char* filecontentSensorsActuators = readFile("../../../Testfiles/SensorsActuators.json");
    Sensor* sensors = parseSensors(filecontentSensorsActuators, strlen(filecontentSensorsActuators), &sensorCount);
    Actuator* actuators = parseActuators(filecontentSensorsActuators, strlen(filecontentSensorsActuators), &actuatorCount);
    char* filecontentInitiatlization = readFile("../../../Testfiles/Initialization.json");
    for (int i = 0; i < sensorCount; i++)
    {
        printSensorData(sensors[i]);
    }
    for (int i = 0; i < actuatorCount; i++)
    {
        printActuatorData(actuators[i]);
    }
    Variable* variables = malloc(sizeof(*variables) * sensorCount);
    for (int i = 0; i < sensorCount; i++)
    {
        variables[i] = (Variable){sensors[i].sensorID, &sensors[i].value};
    }
    StateMachine* stateMachines = parseStateMachines(filecontentInitiatlization, strlen(filecontentInitiatlization), variables, sensorCount, &stateMachineCount);
    for (int i = 0; i < stateMachineCount; i++)
    {
        printStateMachineInfo(&stateMachines[i]);
    }
    destroySensors(sensors, sensorCount);
    destroyActuators(actuators, actuatorCount);
    destroyStateMachines(stateMachines, stateMachineCount);
    free(variables);
    free(filecontentInitiatlization);
    free(filecontentSensorsActuators); 
    return 0;
}