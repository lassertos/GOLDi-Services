#include "BooleanExpressionParser.h"
#include "../logging/log.h"

/* All possible token types used in the shunting-yard-algorithm */
typedef enum TokenTypes
{
    TokenTypeOperand,
    TokenTypeOperator,
    TokenTypeOpeningParentheses
} TokenType;

/*
 *  Tokens are used for parsing the given expression in infix notation to an expression in RPN
 *  type - the type of the token
 *  precedence - if the token is an operator this number represents how strong it binds to its operands (higher number <=> stronger bind)
 *  content - a string representation of the token, could be operator (&,|,!,<,>,=), a number or a name for a sensor / an actuator (not null-terminated)
 *  length - the length of the string representation
 */
typedef struct Token_s
{
    TokenType type;
    unsigned int precedence;
    char* content;
    unsigned int length;
} Token;

static unsigned int isNumber(char *str, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (str[i] - 48 < 0 || str[i] - 48 > 9)
        {
            return 0;
        }
    }
    return 1;
}

static char* parseNumber(char *str, int length)
{
    long long number = 0;
    unsigned int size = sizeof(long long);
    char* result = malloc(size);
    for (int i = 0; i < length; i++)
    {
        number = number * 10 + (str[i] - 48);
    }
    for (int i = 0; i < size/8; i++)
    {
        result[i] = (number << (i * 8)) >> (size - 8);
    }
    free(str);
    return result;
}

static int getOperatorPrecedence(char operator)
{
    switch(operator)
    {
        case '&':
            return 3;
        case '!':
            return 4;
        case '|':
            return 2;
        case '=':
            return 1;
        case '>':
            return 1;
        case '<':
            return 1;
        default:
            return -1;
    }
}

static void destroyToken(Token *token)
{
    free(token->content);
    free(token);
}

/* can be used to print an overview of the queue */
static void printTokenQueue(queue_t* queue)
{
    printf("Current queue:\n");
    if (queue_empty(queue))
    {
        return;
    }
    struct queue_node_s *current = queue->front;
    Token *token = (Token*)current->data;
    while (token != NULL)
    {
        printf("Type: %d, Content: %.*s, Length: %d, Precedence: %d\n", token->type, token->length, token->content, token->length, token->precedence);
        if (current->next != NULL)
            current = current->next;
        else
            break;
        token = (Token*)current->data;
    }
}

/* can be used to print an overview of the stack */
static void printTokenStack(stack_t *stack)
{
    printf("Current stack:\n");
    if (stack_empty(stack))
    {
        return;
    }
    struct stack_frame_s *current = stack->top;
    Token *token = (Token*)current->data;
    while (token != NULL)
    {
        printf("Type: %d, Content: %.*s, Length: %d, Precedence: %d\n", token->type, token->length, token->content, token->length, token->precedence);
        if (current->next != NULL)
            current = current->next;
        else
            break;
        token = (Token*)current->data;
    }
}

/* removes all negation operators that would cancel out each other */
static void removeDoubleNegation(char *str, unsigned int length)
{
    int lastNegation = 0;
    int negationFound = 0;
    for (int i = 0; i < length; i++)
    {
        if (str[i] == '!' && !negationFound)
        {
            negationFound = 1;
            lastNegation = i;
        }
        else if (str[i] == '!' && negationFound)
        {
            negationFound = 0;
            str[i] = ' ';
            str[lastNegation] = ' ';
        }
        else if (str[i] != ' ' && str[i] != '!')
        {
            negationFound = 0;
        }
    }    
}

/* Is called when an operator is found in the given expression TODO: error handling*/
static int shuntingyardReadOperator(char operator, char *firstOperand, unsigned int *sizeFirstOperand, 
                            unsigned int *startIndexFirstOperand, queue_t *queue, stack_t *stack)
{
    int precedence = getOperatorPrecedence(operator);

    if (precedence < 1)
    {
        return 0;
    }

    if (*sizeFirstOperand)
    {
        Token *operandToken = malloc(sizeof(Token));
        operandToken->type = TokenTypeOperand;
        operandToken->content = malloc(*sizeFirstOperand);
        memcpy(operandToken->content, firstOperand, *sizeFirstOperand);
        operandToken->length = *sizeFirstOperand;
        operandToken->precedence = 0;
        queue_enqueue(queue, operandToken);
    }

    Token *operatorToken = malloc(sizeof(Token));
    operatorToken->type = TokenTypeOperator;
    operatorToken->content = malloc(1);
    operatorToken->content[0] = operator;
    operatorToken->length = 1;
    operatorToken->precedence = precedence;

    while ( !stack_empty(stack) && 
            ((Token*)stack->top->data)->type == TokenTypeOperator &&
            operatorToken->precedence <= ((Token*)stack->top->data)->precedence )
    {
        queue_enqueue(queue, stack_pop(stack));
    }
    
    stack_push(stack, operatorToken);

    *startIndexFirstOperand += *sizeFirstOperand+1;
    *sizeFirstOperand = 0;

    return 1;
}

/* A simple implementation of the shunting-yard-algorithm */
static queue_t *shuntingyardAlgorithm(char *str, unsigned int length)
{
    removeDoubleNegation(str, strlen(str));
	queue_t *queue = queue_new();
	stack_t *stack = stack_new();
    char currentValue[length];
    unsigned int sizeCurrentValue = 0;
    unsigned int startIndexCurrentValue = 0;
	for (int i = 0; i < length; i++)
	{
		switch(str[i])
        {
            case '&':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '|':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '<':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '>':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '=':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '!':
                shuntingyardReadOperator(str[i], currentValue, &sizeCurrentValue, &startIndexCurrentValue, queue, stack);
                break;

            case '(':
            {
                Token *token = malloc(sizeof(Token));
                token->type = TokenTypeOpeningParentheses;
                token->content = malloc(1);
                token->content[0] = str[i];
                token->length = 1;
                token->precedence = 0;
                stack_push(stack, token);
                startIndexCurrentValue++;
                break;
            }

            case ')':
            {
                if (sizeCurrentValue > 0)
                {
                    Token *token = malloc(sizeof(Token));
                    token->type = TokenTypeOperand;
                    token->content = malloc(sizeCurrentValue);
                    memcpy(token->content, currentValue, sizeCurrentValue);
                    token->length = sizeCurrentValue;
                    token->precedence = 0;
                    queue_enqueue(queue, token);
                    startIndexCurrentValue += sizeCurrentValue+1;
                    sizeCurrentValue = 0;
                }
                while (!(((Token*)stack->top->data)->type == TokenTypeOpeningParentheses))
                {
                    if(stack_empty(stack))
                    {
                        log_error("BooleanExpressionParser: Too many closing parentheses\n");
                        return NULL;
                    }
                    queue_enqueue(queue, stack_pop(stack));
                }
                destroyToken((Token*)stack_pop(stack));
                break;
            }

            case ' ':
                startIndexCurrentValue++;
                break;

            case '\n':
                if (sizeCurrentValue > 0)
                {
                    Token *token = malloc(sizeof(Token));
                    token->type = TokenTypeOperand;
                    token->content = malloc(sizeCurrentValue);
                    memcpy(token->content, currentValue, sizeCurrentValue);
                    token->length = sizeCurrentValue;
                    token->precedence = 0;
                    queue_enqueue(queue, token);
                }
                break;

            default:
                currentValue[i-startIndexCurrentValue] = str[i];
                sizeCurrentValue++;
        }
        if (i == length-1 && sizeCurrentValue > 0)
        {
            Token *token = malloc(sizeof(Token));
            token->type = TokenTypeOperand;
            token->content = malloc(sizeCurrentValue);
            memcpy(token->content, currentValue, sizeCurrentValue);
            token->length = sizeCurrentValue;
            token->precedence = 0;
            queue_enqueue(queue, token);
        }
	}
    while (!stack_empty(stack))
    {
        if (((Token*)stack->top->data)->type == TokenTypeOpeningParentheses)
        {
            printf("Error: Too many opening parentheses\n");
            return NULL;
        }
        queue_enqueue(queue, stack_pop(stack));
    }

    stack_destroy(stack);
    return queue;
}

void destroyBooleanExpression(BooleanExpression *expression)
{
    if (expression == NULL)
    {
        return;
    }

    destroyBooleanExpression(expression->leftside);
    destroyBooleanExpression(expression->rightside);

    if (expression->name != NULL)
    {
        free(expression->name);
    }

    if (expression->name == NULL && expression->value != NULL)
    {
        free(expression->value);
    }

    free(expression);
}

int inVariables(Token* token, Variable* variables, unsigned int variablesCount)
{
    for (int i = 0; i < variablesCount; i++)
    {
        if (token->length == strlen(variables[i].name))
        {
            unsigned int isEqual = 1;
            for (int j = 0; j < token->length; j++)
            {
                if (token->content[j] != variables[i].name[j])
                {
                    isEqual = 0;
                }
            }
            if (isEqual)
            {
                return i;
            }
        }
    }
    return -1;
}

char* adaptConstant(BooleanExpression* expression)
{
    switch (expression->operandType)
    {
        case OperandTypeBinary:
            expression->valueSize = 1;
            char* result = malloc(1);
            if (!strcmp(expression->value, "1"))
            {
                result[0] = 1;
            }
            else if (!strcmp(expression->value, "0"))
            {
                result[0] = 0;
            }
            else
            {
                free(result);
                return NULL;
            }
            free(expression->value);
            return result;
            break;

        case OperandTypeNumber:
            expression->valueSize = sizeof(long long);
            if (isNumber(expression->value, strlen(expression->value)))
            {
                return parseNumber(expression->value, strlen(expression->value));
            }
            else
            {
                return NULL;
            }
            break;
        
        default:
            return NULL;
    }
}

/* this function is used to parse a boolean expression from a string with given length */
BooleanExpression *parseBooleanExpression(char* str, unsigned int length, Variable* variables, unsigned int variablesCount)
{
    queue_t *queue = shuntingyardAlgorithm(str, length);
    BooleanExpression *expression;
    stack_t *expressionStack = stack_new();

    while (!queue_empty(queue))
    {
        Token *token = (Token*)queue_dequeue(queue);

        switch (token->type)
        {
            case TokenTypeOperator:
            {
                BooleanExpression *operation = malloc(sizeof(BooleanExpression));
                *operation = (BooleanExpression){BoolExprCONSTANT, NULL, NULL, NULL, NULL, NULL, 1, OperandTypeUnknown, OperandTypeBinary};

                if (token->content[0] == '!')
                {
                    operation->type = BoolExprNOT;
                    operation->operandType = OperandTypeBinary;
                    operation->valueSize = 1;
                    operation->leftside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->leftside->parent = operation;
                }
                else
                {
                    switch(token->content[0])
                    {
                        case '&':
                            operation->type = BoolExprAND;
                            operation->operandType = OperandTypeBinary;
                            operation->valueSize = 1;
                            break;
                        case '|':
                            operation->type = BoolExprOR;
                            operation->operandType = OperandTypeBinary;
                            operation->valueSize = 1;
                            break;
                        case '>':
                            operation->type = BoolExprGREATER;
                            operation->operandType = OperandTypeNumber;
                            operation->valueSize = 1;
                            break;
                        case '<':
                            operation->type = BoolExprLOWER;
                            operation->operandType = OperandTypeNumber;
                            operation->valueSize = 1;
                            break;
                        case '=':
                            operation->type = BoolExprEQUAL;
                            operation->operandType = OperandTypeNumber;
                            operation->valueSize = 1;
                            break;
                    }

                    /* get the right child */
                    operation->rightside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->rightside->parent = operation;

                    if (operation->rightside->operandType == OperandTypeUnknown) // right child is a constant
                    {
                        operation->rightside->operandType = operation->operandType;
                        operation->rightside->resultType = operation->operandType;
                        operation->rightside->value = adaptConstant(operation->rightside);
                    }
                    else if (operation->rightside->operandType != operation->operandType)
                    {
                        log_error("BooleanExpressionParser: OperandTypes do not match!");
                        return NULL;
                    }
                    else if (operation->rightside->resultType != operation->operandType)
                    {
                        log_error("BooleanExpressionParser: ResultType and OperandType do not match!");
                        return NULL;
                    }

                    /* get the left child */
                    operation->leftside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->leftside->parent = operation;

                    if (operation->leftside->operandType == OperandTypeUnknown) // left child is a constant
                    {
                        operation->leftside->operandType = operation->operandType;
                        operation->leftside->resultType = operation->operandType;
                        operation->leftside->value = adaptConstant(operation->leftside);
                    }
                    else if (operation->leftside->operandType != operation->operandType)
                    {
                        log_error("BooleanExpressionParser: OperandTypes do not match!");
                        return NULL;
                    }
                    else if (operation->leftside->resultType != operation->operandType)
                    {
                        log_error("BooleanExpressionParser: ResultType and OperandType do not match!");
                        return NULL;
                    }
                }
                stack_push(expressionStack, operation);
                break;
            }

            case TokenTypeOperand:
            {
                BooleanExpression *leaf = malloc(sizeof(BooleanExpression));
                *leaf = (BooleanExpression){BoolExprCONSTANT, NULL, NULL, leaf, NULL, NULL, 0, OperandTypeUnknown, OperandTypeUnknown};

                int index = inVariables(token, variables, variablesCount);
                if (index != -1)
                {
                    leaf->type = BoolExprVARIABLE;
                    leaf->resultType = OperandTypeBinary;
                    leaf->operandType = variables[index].type;
                    leaf->name = malloc(token->length+1);
                    memcpy(leaf->name, token->content, token->length);
                    leaf->name[token->length] = '\0';
                    leaf->value = variables[index].value;
                    leaf->valueSize = variables[index].valueSize;
                    if (leaf->value == NULL)
                    {
                        return NULL;
                    }
                }
                else
                {
                    leaf->value = malloc(token->length+1);
                    memcpy(leaf->value, token->content, token->length);
                    leaf->value[token->length] = '\0';
                }

                stack_push(expressionStack, leaf);
                break;
            }
            
            default:
                break;
        }
        destroyToken(token);
    }

    expression = stack_pop(expressionStack);
    queue_destroy(queue);
    stack_destroy(expressionStack);

    return expression;
}

void printBooleanExpression(BooleanExpression *expression)
{
    if (expression == NULL)
    {
        printf("The BooleanExpression points to NULL\n");
        return;
    }
    if (expression->type == BoolExprCONSTANT)
    {
        if (expression->name != NULL)
            printf("%s:", expression->name);
        for (int i = 0; i < expression->valueSize; i++)
        {
            if (!(i == expression->valueSize-1)) 
            {
                printf("%d, ", expression->value[i]);
            }
            else
            {
                printf("%d]", expression->value[i]);
            }
        }
    }
    else if (expression->type == BoolExprVARIABLE)
    {
        if (expression->name != NULL)
            printf("%s:", expression->name);
        for (int i = 0; i < expression->valueSize; i++)
        {
            if (!(i == expression->valueSize-1)) 
            {
                printf("%d, ", expression->value[i]);
            }
            else
            {
                printf("%d]", expression->value[i]);
            }
        }
    }
    else if (expression->type == BoolExprNOT)
    {
        printf("!");
        printBooleanExpression(expression->leftside);
    }
    else 
    {
        printf("(");
        printBooleanExpression(expression->leftside);
        switch(expression->type)
        {
            case BoolExprAND:
                printf(" and ");
                break;
            case BoolExprOR:
                printf(" or ");
                break;
            case BoolExprLOWER:
                printf(" < ");
                break;
            case BoolExprGREATER:
                printf(" > ");
                break;
            case BoolExprEQUAL:
                printf(" == ");

        }
        printBooleanExpression(expression->rightside);
        printf(")");
    }
    if (expression->parent == expression)
    {
        printf("\n");
    }
}

char* calculateValue(BooleanExpression* expression)
{
    char* result1;
    char* result2;
    char* result;
    switch (expression->type)
    {
        case BoolExprCONSTANT:
        {
            if (expression->value != NULL)
            {
                return expression->value;
            }
            return NULL;
        }

        case BoolExprVARIABLE:
        {
            if (expression->value != NULL)
            {
                return expression->value;
            }
            return NULL;
        }

        case BoolExprAND:
        {
            result1 = calculateValue(expression->leftside);
            result2 = calculateValue(expression->rightside);
            if (result1 == NULL || result2 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            if (result1[0] && result2[0])
            {
                result[0] = 1;
            }
            else 
            {
                result[0] = 0;
            }
            return result;
        }

        case BoolExprOR:
        {
            result1 = calculateValue(expression->leftside);
            result2 = calculateValue(expression->rightside);
            if (result1 == NULL || result2 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            if (result1[0] || result2[0])
            {
                result[0] = 1;
            }
            else 
            {
                result[0] = 0;
            }
            return result;
        }

        case BoolExprNOT:
        {
            result1 = calculateValue(expression->leftside);
            if (result1 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            if (!result1[0])
            {
                result[0] = 1;
            }
            else 
            {
                result[0] = 0;
            }
            return result;
        }

        case BoolExprEQUAL:
        {
            result1 = calculateValue(expression->leftside);
            result2 = calculateValue(expression->rightside);
            unsigned int result1Size = expression->leftside->valueSize;
            unsigned int result2Size = expression->rightside->valueSize;
            if (result1 == NULL || result2 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            result[0] = 1;
            if (result1Size > result2Size)
            {
                for (int i = 0; i < result1Size - result2Size; i++)
                {
                    if (result1[i] != 0)
                    {
                        result[0] = 0;
                        return result;
                    }
                }
                for (int i = result1Size - result2Size; i < result1Size; i++)
                {
                    int j = i - (result1Size - result2Size);
                    if (result1[i] != result2[j])
                    {
                        result[0] = 0;
                        return result;
                    }
                }
            }
            else 
            {
                for (int i = 0; i < result2Size - result1Size; i++)
                {
                    if (result2[i] != 0)
                    {
                        result[0] = 0;
                        return result;
                    }
                }
                for (int i = result2Size - result1Size; i < result2Size; i++)
                {
                    int j = i - (result2Size - result1Size);
                    if (result1[i] != result2[j])
                    {
                        result[0] = 0;
                        return result;
                    }
                }
            }
            return result;
        }

        case BoolExprGREATER:
        {
            result1 = calculateValue(expression->leftside);
            result2 = calculateValue(expression->rightside);
            unsigned int result1Size = expression->leftside->valueSize;
            unsigned int result2Size = expression->rightside->valueSize;
            if (result1 == NULL || result2 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            result[0] = 0;
            if (result1Size > result2Size)
            {
                for (int i = 0; i < result1Size - result2Size; i++)
                {
                    if (result1[i] != 0)
                    {
                        result[0] = 1;
                        return result;
                    }
                }
                for (int i = result1Size - result2Size; i < result1Size; i++)
                {
                    int j = i - (result1Size - result2Size);
                    if (result1[i] > result2[j])
                    {
                        result[0] = 1;
                        return result;
                    }
                    else if (result1[i] < result2[j])
                    {
                        return result;
                    }
                }
            }
            else 
            {
                for (int i = 0; i < result2Size - result1Size; i++)
                {
                    if (result2[i] != 0)
                    {
                        return result;
                    }
                }
                for (int i = result2Size - result1Size; i < result2Size; i++)
                {
                    int j = i - (result2Size - result1Size);
                    if (result1[i] > result2[j])
                    {
                        result[0] = 1;
                        return result;
                    }
                    else if (result1[i] < result2[j])
                    {
                        return result;
                    }
                }
            }
            return result;
        }

        case BoolExprLOWER:
        {
            result1 = calculateValue(expression->leftside);
            result2 = calculateValue(expression->rightside);
            unsigned int result1Size = expression->leftside->valueSize;
            unsigned int result2Size = expression->rightside->valueSize;
            if (result1 == NULL || result2 == NULL)
            {
                return NULL;
            }
            result = malloc(1);
            result[0] = 0;
            if (result1Size > result2Size)
            {
                for (int i = 0; i < result1Size - result2Size; i++)
                {
                    if (result1[i] != 0)
                    {
                        return result;
                    }
                }
                for (int i = result1Size - result2Size; i < result1Size; i++)
                {
                    int j = i - (result1Size - result2Size);
                    if (result1[i] < result2[j])
                    {
                        result[0] = 1;
                        return result;
                    }
                    else if (result1[i] > result2[j])
                    {
                        return result;
                    }
                }
            }
            else 
            {
                for (int i = 0; i < result2Size - result1Size; i++)
                {
                    if (result2[i] != 0)
                    {
                        result[0] = 1;
                        return result;
                    }
                }
                for (int i = result2Size - result1Size; i < result2Size; i++)
                {
                    int j = i - (result2Size - result1Size);
                    if (result1[i] < result2[j])
                    {
                        result[0] = 1;
                        return result;
                    }
                    else if (result1[i] > result2[j])
                    {
                        return result;
                    }
                }
            }
            return result;
        }
        
        default:
            break;
    }
}

int evaluateBooleanExpression(BooleanExpression *expression)
{
    if (expression->resultType != OperandTypeBinary || expression->valueSize != 1)
    {
        return -1;
    }

    char* result = calculateValue(expression);

    if (result == NULL)
    {
        return -1;
    }

    if (*result = 1)
    {
        return 1;
    }
    else 
    {
        return 0;
    }
}