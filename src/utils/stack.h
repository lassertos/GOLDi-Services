#define SUCCESS 0
#define ERR_INVAL 1
#define ERR_NOMEM 2

#define FALSE 0
#define TRUE 1

typedef struct stack_s stack_t;

struct stack_frame_s {
  struct stack_frame_s *next;
  void *data;
};

struct stack_s {
  struct stack_frame_s *top;
};

int stack_destroy(stack_t *stack);
int stack_empty(stack_t *stack);
stack_t *stack_new(void);
void *stack_pop(stack_t *stack);
int stack_push(stack_t *stack, void *data);