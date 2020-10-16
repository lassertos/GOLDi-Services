#include "ExpressionParsers.h"

MathematicalExpression *createMathematicalExpression(const char *expression, const Variable *variables, int var_count, int *error)
{
    return te_compile(expression, variables, var_count, error);
}

double evaluateMathematicalExpression(const MathematicalExpression *expression)
{
    return te_eval(expression);
}

void destroyMathematicalExpression(MathematicalExpression *expression)
{
    te_free(expression);
}