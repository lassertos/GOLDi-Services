#define SUCCESS 0
#define ERR_INVAL 1
#define ERR_NOMEM 2

#define FALSE 0
#define TRUE 1

typedef struct stack_s stack_dt;

struct stack_frame_s {
  struct stack_frame_s *next;
  void *data;
};

struct stack_s {
  struct stack_frame_s *top;
};

int stack_destroy(stack_dt *stack);
int stack_empty(stack_dt *stack);
stack_dt *stack_new(void);
void *stack_pop(stack_dt *stack);
int stack_push(stack_dt *stack, void *data);