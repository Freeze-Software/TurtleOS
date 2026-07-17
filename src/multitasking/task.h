#include <stdint.h>

typedef struct {
    int pid;
    char name[32];
    int active;
} task_info_t;

void tasks_get_info(task_info_t* input, int max_tasks);
int add_task(void (*func), char* name, int stack_size);
int disable_task(int id);
int enable_task(int id);
extern volatile uint32_t pit_ticks;
int get_task_count();
