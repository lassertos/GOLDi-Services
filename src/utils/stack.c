#include "stack.h"
#include <stdlib.h>

int stack_destroy(stack_dt *stack) 
{
    if (stack == NULL)
    {
        return ERR_INVAL;
    }

    while (stack->top != NULL) 
    {
        struct stack_frame_s *frame = stack->top;
        stack->top = frame->next;
        free(frame);
    }

    free(stack);
    return SUCCESS;
}


int stack_empty(stack_dt *stack) 
{

    if (stack == NULL || stack->top == NULL) 
    {
        return TRUE;
    } 
    else 
    {
        return FALSE;
    }

}


stack_dt *stack_new(void) 
{
    stack_dt *stack = malloc(sizeof(*stack));

    if (stack == NULL)  
    {
        return NULL;
    }

    stack->top = NULL;
    return stack;
}


void *stack_pop(stack_dt *stack) 
{
    if (stack == NULL || stack->top == NULL) 
    {
        return NULL;
    }

    struct stack_frame_s *frame = stack->top;
    void *data = frame->data;
    stack->top = frame->next;
    free(frame);
    return data;
}


int stack_push(stack_dt *stack, void *data) 
{
    if (stack == NULL) 
    {
        return ERR_INVAL;
    }

    struct stack_frame_s *frame = malloc(sizeof(*frame));

    if (frame == NULL) 
    {
        return ERR_NOMEM;
    }

    frame->data = data;
    frame->next = stack->top;
    stack->top = frame;
    return SUCCESS;
}