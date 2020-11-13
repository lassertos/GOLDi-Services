#include "StateMachine.h"
#include "json.h"
#include "string.h"

static StateMachineState* getStateMachineStateByName(char* name, StateMachine* stateMachine)
{
    for (int i = 0; i < stateMachine->statesCount; i++)
    {
        if (!strcmp(name, stateMachine->states[i].name))
        {
            return &stateMachine->states[i];
        }
    }
    printf("State with name %s not found\n", name);
    return NULL;
}

StateMachine* parseStateMachines(char* string, unsigned int length, Variable* variables, unsigned int variablesCount, unsigned int* stateMachineCount)
{
    JSON* json = JSONParse(string);
    *stateMachineCount = JSONGetArraySize(json);
    StateMachine* stateMachines = malloc(sizeof(*stateMachines) * (*stateMachineCount));
    JSON* jsonStateMachine = NULL;

    int stateMachineIndex = 0;
    JSONArrayForEach(jsonStateMachine, json)
    {
        stateMachines[stateMachineIndex].name = malloc(strlen(jsonStateMachine->string)+1);
        strcpy(stateMachines[stateMachineIndex].name, jsonStateMachine->string);
        JSON* jsonStateNames = JSONGetObjectItem(jsonStateMachine, "StateNames");
        stateMachines[stateMachineIndex].statesCount = JSONGetArraySize(jsonStateNames);

        const JSON* jsonStateName = NULL;
        unsigned int currentIndex = 0;
        unsigned int variablesCountNew = variablesCount + JSONGetArraySize(jsonStateNames);
        Variable* variablesNew = malloc(sizeof(*variablesNew) * variablesCountNew);
        for (int i = 0; i < variablesCount; i++)
        {
            variablesNew[i] = variables[i];
        }
        stateMachines[stateMachineIndex].states = malloc(sizeof(*stateMachines[stateMachineIndex].states) * stateMachines[stateMachineIndex].statesCount);
        JSONArrayForEach(jsonStateName, jsonStateNames)
        {
            stateMachines[stateMachineIndex].states[currentIndex].name = malloc(strlen(jsonStateName->valuestring)+1);
            strcpy(stateMachines[stateMachineIndex].states[currentIndex].name, jsonStateName->valuestring);
            stateMachines[stateMachineIndex].states[currentIndex].isActive = 0;
            stateMachines[stateMachineIndex].states[currentIndex].outputs = NULL;
            stateMachines[stateMachineIndex].states[currentIndex].outputCount = 0;
            variablesNew[variablesCount+currentIndex] = (Variable){stateMachines[stateMachineIndex].states[currentIndex].name, &stateMachines[stateMachineIndex].states[currentIndex].isActive};
            currentIndex++;
        }
        
        char* startStateName = JSONGetObjectItem(jsonStateMachine, "StartState")->valuestring;
        char* endStateName = JSONGetObjectItem(jsonStateMachine, "EndState")->valuestring;
        for (int i = 0; i < stateMachines[stateMachineIndex].statesCount; i++)
        {
            if (!strcmp(startStateName, stateMachines[stateMachineIndex].states[i].name))
            {
                stateMachines[stateMachineIndex].startState = &stateMachines[stateMachineIndex].states[i];
                stateMachines[stateMachineIndex].activeState = &stateMachines[stateMachineIndex].states[i];
                stateMachines[stateMachineIndex].states[i].isActive = 1;
            }
            else if (!strcmp(endStateName, stateMachines[stateMachineIndex].states[i].name))
            {
                stateMachines[stateMachineIndex].endState = &stateMachines[stateMachineIndex].states[i];
            }
        }

        JSON* jsonStateEquations = JSONGetObjectItem(jsonStateMachine, "StateTransitionFunctions");
        JSON* jsonStateEquation = NULL;
        JSONArrayForEach(jsonStateEquation, jsonStateEquations)
        {
            StateMachineState* currentState = getStateMachineStateByName(jsonStateEquation->string, &stateMachines[stateMachineIndex]);
            currentState->transitionFunction = parseBooleanExpression(jsonStateEquation->valuestring, strlen(jsonStateEquation->valuestring), variablesNew, variablesCountNew);
        }

        JSON* jsonStateOutputs = JSONGetObjectItem(jsonStateMachine, "StateOutputs");
        JSON* jsonStateOutput = NULL;
        JSONArrayForEach(jsonStateOutput, jsonStateOutputs)
        {
            StateMachineState* currentState = getStateMachineStateByName(jsonStateOutput->string, &stateMachines[stateMachineIndex]);
            unsigned int outputIndex = 0;
            currentState->outputCount = JSONGetArraySize(jsonStateOutput);
            currentState->outputs = malloc(sizeof(*(currentState->outputs)) * currentState->outputCount);
            JSON* jsonStateOutputValue = NULL;
            JSONArrayForEach(jsonStateOutputValue, jsonStateOutput)
            {
                currentState->outputs[outputIndex].actuatorID = malloc(strlen(jsonStateOutputValue->string)+1);
                strcpy(currentState->outputs[outputIndex].actuatorID, jsonStateOutputValue->string);
                currentState->outputs[outputIndex].value = (long long)JSONGetNumberValue(jsonStateOutputValue);
            }
        }

        free(variablesNew);
        stateMachineIndex++;
    }

    JSONDelete(json);
    return stateMachines;
}

StateMachine* getStateMachineByName(char* name, StateMachine* stateMachines, unsigned int stateMachineCount)
{
    for (int i = 0; i < stateMachineCount; i++)
    {
        if (!strcmp(stateMachines[i].name, name))
        {
            return &stateMachines[i];
        }
    }
    return NULL;
}

int updateStateMachine(StateMachine* stateMachine)
{
    for (int i = 0; i < stateMachine->statesCount; i++)
    {
        int resultTransitionFunction = evaluateBooleanExpression(stateMachine->states[i].transitionFunction);
        if (resultTransitionFunction != -1)
        {
            stateMachine->states[i].isActive = resultTransitionFunction;
            stateMachine->activeState = &stateMachine->states[i];
        }
        else
        {
            return 1;
        }
    }

    return 0;
}

int executeStateMachine(StateMachineExecution* execution)
{
    while(strcmp(execution->stateMachine->activeState->name, execution->stateMachine->endState->name) != 0)
    {
        if (execution->stopped)
        {
            return 0;
        }
        if (updateStateMachine(execution->stateMachine))
        {
            execution->stopped = 1;
            return 1;
        }
    }
    execution->stopped = 1;
    return 0;
}

void resetStateMachine(StateMachine* stateMachine)
{
    for (int i = 0; i < stateMachine->statesCount; i++)
    {
        stateMachine->states[i].isActive = 0;
    }
    stateMachine->startState->isActive = 1;
    stateMachine->activeState = stateMachine->startState;
}

void destroyStateMachines(StateMachine* stateMachines, unsigned int stateMachineCount)
{
    for (int i = 0; i < stateMachineCount; i++)
    {
        for (int j = 0; j < stateMachines[i].statesCount; j++)
        {
            destroyBooleanExpression(stateMachines[i].states[j].transitionFunction);
            free(stateMachines[i].states[j].name);
            if (stateMachines[i].states[j].outputCount > 0)
            {
                for (int k = 0; k < stateMachines[i].states[j].outputCount; k++)
                {
                    free(stateMachines[i].states[j].outputs[k].actuatorID);
                }
                free(stateMachines[i].states[j].outputs);
            }
        }
        free(stateMachines[i].name);
        free(stateMachines[i].states);
    }
    free(stateMachines);
}

void printStateMachineInfo(StateMachine* stateMachine)
{
    printf("State Machine %s:\n", stateMachine->name);
    printf("{\n");
    printf("    Start State:    %s\n", stateMachine->startState->name);
    printf("    Active State:   %s\n", stateMachine->activeState->name);
    printf("    End State:      %s\n", stateMachine->endState->name);
    for (int i = 0; i < stateMachine->statesCount; i++)
    {
        printf("    State %s:\n", stateMachine->states[i].name);
        printf("    {\n");
        printf("        State Value: %lld\n", stateMachine->states[i].isActive);
        printf("        State Transition Function: ");
        printBooleanExpression(stateMachine->states[i].transitionFunction);
        printf("        State Outputs: \n");
        printf("        {\n");
        for (int j = 0; j < stateMachine->states[i].outputCount; j++)
        {
            printf("            %s: %lld\n", stateMachine->states[i].outputs[j].actuatorID, stateMachine->states[i].outputs[j].value);
        }
        printf("        }\n");
        printf("    }\n");
    }
    printf("}\n");
}