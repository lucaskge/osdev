#ifndef TASK_H
#define TASK_H

#define TASK_STATE_READY   0
#define TASK_STATE_RUNNING 1

// O Bloco de Controle de Processo (PCB) do MeuOS
struct task_control_block {
    int id;                             // ID único do processo (PID)
    const char* name;                   // Nome legível da tarefa
    unsigned int esp;                   // Ponteiro de pilha atual (salvo na troca)
    unsigned int stack_start;           // Endereço base alocado para a pilha no Heap
    int state;                          // Estado atual (Pronto, Rodando)
    struct task_control_block* next;    // Ponteiro para a próxima tarefa (Lista Encadeada)
};

void task_init(void);
// Nova função para lançar processos dinamicamente informando o ponteiro da função
int task_create(const char* name, void (*entry_point)(void));
void start_multitasking(void);
void task_yield(void);

#endif