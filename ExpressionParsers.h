#ifndef EXPRESSIONPARSERS_H
#define EXPRESSIONPARSERS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stack.h"
#include "queue.h"
#include "tinyexpr.h"

/* Common Fields */

typedef te_variable Variable;

/* Mathematical Expressions */

typedef te_expr MathematicalExpression;

MathematicalExpression *createMathematicalExpression(const char *expression, const Variable *variables, int var_count, int *error);
double evaluateMathematicalExpression(const MathematicalExpression *expression);
void destroyMathematicalExpression(MathematicalExpression *expression);

/* Boolean Expressions */

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

BooleanExpression *parseBooleanExpression(char* str, Variable *variables, unsigned int length);
unsigned int evaluateBooleanExpression(BooleanExpression *expression);
void printBooleanExpression(BooleanExpression *expression);
void destroyBooleanExpression(BooleanExpression *expression);

#endif