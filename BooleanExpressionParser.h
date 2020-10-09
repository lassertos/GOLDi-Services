#ifndef BOOLEANEXPRESSIONPARSER_H
#define BOOLEANEXPRESSIONPARSER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stack.h"
#include "queue.h"

typedef struct BooleanExpression_s BooleanExpression;

typedef enum BooleanExpressionTypes
{
    BoolExprAND,
    BoolExprOR,
    BoolExprGREATER,
    BoolExprLOWER,
    BoolExprEQUAL,
    BoolExprNOT,
    BoolExprLEAF
} BooleanExpressionType;

struct BooleanExpression_s
{
    unsigned int type;
    BooleanExpression *leftside;
    BooleanExpression *rightside;
    BooleanExpression *parent;
    unsigned char *name;
    unsigned long long *result;
};

#endif