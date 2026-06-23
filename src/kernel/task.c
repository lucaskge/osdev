#include "task.h"
#include "vga.h"

#define MAX_TASKS 2
#define STACK_SIZE 1024

struct task task_list[MAX_TASKS];
int current_task_idx = 0;

// Pilhas dedicadas alinhadas por hardware a 16 bytes na memória RAM
unsigned int task1_stack[STACK_SIZE] __attribute__((aligned(16)));
unsigned int task2_stack[STACK_SIZE] __attribute__((aligned(16)));

unsigned int* current_task_esp_ptr = 0;
unsigned int next_task_esp_val = 0;
unsigned int shell_stack_backup = 0;

extern void switch_context_asm(void);

void run_scheduler(void) {
    int old_idx = current_task_idx;
    current_task_idx = (current_task_idx + 1) % MAX_TASKS;

    // Delay bruto controlado para a alternância não estourar a taxa de atualização do QEMU
    for(volatile int i = 0; i < 9000000; i++);

    current_task_esp_ptr = &task_list[old_idx].esp;
    next_task_esp_val = task_list[current_task_idx].esp;

    switch_context_asm();
}

// Tarefa 1: Imprime e cede o controle manualmente
void task1_code(void) {
    while(1) {
        terminal_print("[T1] ");
        run_scheduler();
    }
}

// Tarefa 2: Imprime e cede o controle de volta
void task2_code(void) {
    while(1) {
        terminal_print("[T2] ");
        run_scheduler();
    }
}

void task_init(void) {
    // --- CONFIGURAÇÃO DA TAREFA 1 ---
    task_list[0].id = 1;
    task_list[0].state = 0;
    unsigned int* stack1 = &task1_stack[STACK_SIZE - 1];
    
    *stack1 = (unsigned int)task1_code;     // EIP inicial
    *(--stack1) = 0x08;                     // CS
    *(--stack1) = 0x0202;                   // EFLAGS
    
    for(int i = 0; i < 8; i++) {
        *(--stack1) = 0;                    // Zera registradores gerais (pusha)
    }
    task_list[0].esp = (unsigned int)stack1;

    // --- CONFIGURAÇÃO DA TAREFA 2 ---
    task_list[1].id = 2;
    task_list[1].state = 0;
    unsigned int* stack2 = &task2_stack[STACK_SIZE - 1];
    
    *stack2 = (unsigned int)task2_code;     // EIP inicial
    *(--stack2) = 0x08;                     // CS
    *(--stack2) = 0x0202;                   // EFLAGS
    
    for(int i = 0; i < 8; i++) {
        *(--stack2) = 0;                    // Zera registradores gerais (pusha)
    }
    task_list[1].esp = (unsigned int)stack2;
}

void start_multitasking(void) {
    current_task_idx = 0; // Força a primeira tarefa a ser a T1 (índice 0)
    
    current_task_esp_ptr = &shell_stack_backup; // Salva a pilha do Shell fora do array
    next_task_esp_val = task_list[0].esp;       // Carrega a pilha estável da Tarefa 1

    switch_context_asm();
}