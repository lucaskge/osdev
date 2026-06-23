#ifndef TASK_H
#define TASK_H

struct task {
    unsigned int esp;          // Ponteiro da pilha da tarefa
    unsigned int id;           // ID da tarefa
    unsigned int state;        // Estado (0 = pronta)
};

void task_init(void);
void run_scheduler(void);
void start_multitasking(void);

#endif