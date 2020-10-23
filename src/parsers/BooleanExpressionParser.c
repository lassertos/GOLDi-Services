#include "ExpressionParsers.h"

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

static unsigned long long parseNumber(char *str, int length)
{
    int result = 0;
    for (int i = 0; i < length; i++)
    {
        result = result * 10 + (str[i] - 48);
    }
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
                        printf("Error: Too many closing parentheses\n");
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

    if (expression->name == NULL && expression->result != NULL)
    {
        free(expression->result);
    }

    free(expression);
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
                *operation = (BooleanExpression){BoolExprLEAF, NULL, NULL, operation, NULL, NULL};

                if (token->content[0] == '!')
                {
                    operation->type = BoolExprNOT;
                    operation->leftside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->leftside->parent = operation;
                }
                else
                {
                    switch(token->content[0])
                    {
                        case '&':
                            operation->type = BoolExprAND;
                            break;
                        case '|':
                            operation->type = BoolExprOR;
                            break;
                        case '>':
                            operation->type = BoolExprGREATER;
                            break;
                        case '<':
                            operation->type = BoolExprLOWER;
                            break;
                        case '=':
                            operation->type = BoolExprEQUAL;
                            break;
                    }
                    operation->rightside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->rightside->parent = operation;
                    operation->leftside = (BooleanExpression*)stack_pop(expressionStack);
                    operation->leftside->parent = operation;
                }
                stack_push(expressionStack, operation);
                break;
            }

            case TokenTypeOperand:
            {
                BooleanExpression *leaf = malloc(sizeof(BooleanExpression));
                *leaf = (BooleanExpression){BoolExprLEAF, NULL, NULL, leaf, NULL, NULL};

                if (isNumber(token->content, token->length))
                {
                    leaf->result = malloc(sizeof(long long));
                    *leaf->result = parseNumber(token->content, token->length);
                }
                else
                {
                    leaf->name = malloc(token->length+1);
                    memcpy(leaf->name, token->content, token->length);
                    leaf->name[token->length] = '\0';
                    for (int i = 0; i < variablesCount; i++)
                    {
                        if (!strcmp(variables[i].name, leaf->name))
                        {
                            leaf->result = (long long*)variables[i].address;
                        }
                    }
                    if (leaf->result == NULL)
                    {
                        return NULL;
                    }
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
    if (expression->type == BoolExprLEAF)
    {
        if (expression->name != NULL)
            printf("%s:", expression->name);
        printf("%lld", *expression->result);
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

int evaluateBooleanExpression(BooleanExpression *expression)
{
    int result1 = 0;
    int result2 = 0;
    switch (expression->type)
    {
    case BoolExprLEAF:
        if (expression->result != NULL)
        {
            return *expression->result;
        }
        return -1;
        break;

    case BoolExprAND:
        result1 = evaluateBooleanExpression(expression->leftside);
        result2 = evaluateBooleanExpression(expression->rightside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return result1 && result2;
        break;

    case BoolExprOR:
        result1 = evaluateBooleanExpression(expression->leftside);
        result2 = evaluateBooleanExpression(expression->rightside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return result1 || result2;
        break;

    case BoolExprNOT:
        result1 = evaluateBooleanExpression(expression->leftside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return !result1;
        break;

    case BoolExprEQUAL:
        result1 = evaluateBooleanExpression(expression->leftside);
        result2 = evaluateBooleanExpression(expression->rightside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return result1 == result2;
        break;

    case BoolExprGREATER:
        result1 = evaluateBooleanExpression(expression->leftside);
        result2 = evaluateBooleanExpression(expression->rightside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return result1 > result2;
        break;

    case BoolExprLOWER:
        result1 = evaluateBooleanExpression(expression->leftside);
        result2 = evaluateBooleanExpression(expression->rightside);
        if (result1 == -1 || result2 == -1)
        {
            return -1;
        }
        return result1 < result2;
        break;
    
    default:
        break;
    }

}