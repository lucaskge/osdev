#include "task.h"
#include "mem.h" // Importante: Usa o seu alocador de memória RAM
#include "vga.h"

// Ponteiros globais que o seu switch_context_asm precisa ler/escrever
unsigned int current_task_esp_ptr = 0;
unsigned int next_task_esp_val = 0;

static struct task_control_block* head_task = 0;
static struct task_control_block* current_task = 0;
static int next_pid = 1;

extern void switch_context_asm(void);

void task_init(void) {
    // Cria a Tarefa Principal (Main/Shell) de forma estática para servir de base
    struct task_control_block* main_task = (struct task_control_block*)kmalloc(sizeof(struct task_control_block));
    main_task->id = 0;
    main_task->name = "System Shell";
    main_task->state = TASK_STATE_RUNNING;
    main_task->next = main_task; // Lista circular encadeada nela mesma inicialmente
    
    head_task = main_task;
    current_task = main_task;
    
    terminal_print("Subsistema de processos dinamicos alocado.\n");
}

int task_create(const char* name, void (*entry_point)(void)) {
    // 1. Aloca a estrutura PCB da tarefa no Heap
    struct task_control_block* new_task = (struct task_control_block*)kmalloc(sizeof(struct task_control_block));
    if (new_task == 0) return -1; // Sem RAM para o processo

    // 2. Aloca 4KB de espaço de Pilha (Stack) exclusivo para esta tarefa no Heap
    unsigned int stack_alloc = (unsigned int)kmalloc(4096);
    if (stack_alloc == 0) {
        kfree(new_task);
        return -1;
    }

    new_task->id = next_pid++;
    new_task->name = name;
    new_task->stack_start = stack_alloc;
    new_task->state = TASK_STATE_READY;

    // 3. Simula o estado inicial da Pilha para o 'switch_context_asm' desempilhar corretamente
    // O topo da pilha começa no fim do bloco de 4KB alocado (pilha cresce para baixo)
    unsigned int* stack_ptr = (unsigned int*)(stack_alloc + 4096);

    // Empurra o contexto inicial idêntico ao esperado pelo iret/ret e pusha/pushf do seu Assembly:
    stack_ptr--; *stack_ptr = (unsigned int)entry_point; // EIP (Ponto de entrada da função)
    stack_ptr--; *stack_ptr = 0x08;                     // CS de controle (Código do Kernel)
    stack_ptr--; *stack_ptr = 0x0202;                   // EFLAGS padrão com interrupções ativas
    
    // Empurra os 8 registradores gerais zerados (simulando o pusha inicial)
    for (int i = 0; i < 8; i++) {
        stack_ptr--;
        *stack_ptr = 0;
    }

    new_task->esp = (unsigned int)stack_ptr; // Guarda o ponteiro resultante no PCB

    // 4. Insere a nova tarefa na estrutura circular encadeada
    new_task->next = head_task->next;
    head_task->next = new_task;

    return new_task->id;
}

// Passa o controle voluntariamente para o próximo processo da lista circular
void task_yield(void) {
    if (current_task == 0 || current_task->next == current_task) return;

    struct task_control_block* old_task = current_task;
    struct task_control_block* next_task = current_task->next;

    current_task = next_task;
    old_task->state = TASK_STATE_READY;
    current_task->state = TASK_STATE_RUNNING;

    // Configura os endereços físicos que o switch_context_asm no boot.asm vai usar
    current_task_esp_ptr = (unsigned int)&old_task->esp;
    next_task_esp_val = next_task->esp;

    switch_context_asm(); // Dispara o chaveamento via hardware!
}

void start_multitasking(void) {
    terminal_print("Multitarefa Dinamica iniciada. Chaveando contextos...\n");
    task_yield();
}