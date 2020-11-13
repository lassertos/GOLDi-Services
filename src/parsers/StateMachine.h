#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "ExpressionParsers.h"

typedef struct
{
    char*       actuatorID;
    long long   value;  
} StateMachineOutput;

typedef struct 
{
    char*               name;
    BooleanExpression*  transitionFunction;
    StateMachineOutput* outputs;
    unsigned int        outputCount;
    unsigned long long  isActive; //long long because it can be a part of a Boolean Expression needs to be tested if it can be made smaller
} StateMachineState;

typedef struct 
{
    char*               name;
    StateMachineState*  states;
    unsigned int        statesCount;
    StateMachineState*  startState;
    StateMachineState*  activeState;
    StateMachineState*  endState;
} StateMachine;

typedef struct
{
    StateMachine*   stateMachine;
    unsigned int    stopped:1;
} StateMachineExecution;


StateMachine* parseStateMachines(char* string, unsigned int length, Variable* variables, unsigned int variablesCount, unsigned int* stateMachineCount);
StateMachine* getStateMachineByName(char* name, StateMachine* stateMachines, unsigned int stateMachineCount);
int updateStateMachine(StateMachine* stateMachine);
void resetStateMachine(StateMachine* stateMachine);
void destroyStateMachines(StateMachine* stateMachine, unsigned int stateMachineCount);
void printStateMachineInfo(StateMachine* StateMachine);
int executeStateMachine(StateMachineExecution* execution);

#endif