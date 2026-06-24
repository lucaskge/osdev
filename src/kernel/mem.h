#ifndef MEM_H
#define MEM_H

// Estrutura de metadados de 8 bytes que fica invisível logo atrás de cada bloco de RAM
struct mem_header {
    unsigned int size;          // Tamanho do bloco de dados úteis
    unsigned char is_free;      // 1 se o bloco estiver livre, 0 se estiver ocupado
    struct mem_header* next;    // Ponteiro para o próximo bloco da lista encadeada
};

void mem_init(void);
void* kmalloc(unsigned int size);
void kfree(void* ptr);

// Função para retornar quanta memória RAM total o Heap possui livre
unsigned int mem_get_free_space(void);

#endif