#ifndef EXPRESSIONPARSERS_H
#define EXPRESSIONPARSERS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../utils/stack.h"
#include "../utils/queue.h"

typedef struct {
    int type;
    const char* name;
    const char* value;
    unsigned int valueSize;
} Variable;

typedef struct BooleanExpression_s BooleanExpression;

typedef enum BooleanExpressionTypes
{
    BoolExprAND,
    BoolExprOR,
    BoolExprGREATER,
    BoolExprLOWER,
    BoolExprEQUAL,
    BoolExprNOT,
    BoolExprCONSTANT,
    BoolExprVARIABLE
} BooleanExpressionType;

typedef enum OperandTypes
{
    OperandTypeUnknown,
    OperandTypeBinary,
    OperandTypeNumber
} OperandType;

struct BooleanExpression_s
{
    unsigned int        type;
    BooleanExpression*  leftside;
    BooleanExpression*  rightside;
    BooleanExpression*  parent;
    unsigned char*      name;
    char*               value;
    unsigned int        valueSize;
    OperandType         operandType;
    OperandType         resultType;
};

BooleanExpression *parseBooleanExpression(char* str, unsigned int length, Variable* variables, unsigned int variablesCount);
int evaluateBooleanExpression(BooleanExpression* expression);
void printBooleanExpression(BooleanExpression* expression);
void destroyBooleanExpression(BooleanExpression* expression);

#endif