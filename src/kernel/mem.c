#include "mem.h"

// Define o início do Heap em 2MB (0x200000), totalmente dentro dos 4MB mapeados pela sua paginação
#define HEAP_START 0x200000
// Reserva 1MB de espaço para o Heap (o suficiente para criar milhares de blocos)
#define HEAP_SIZE  0x100000

static struct mem_header* first_block = (struct mem_header*)HEAP_START;

void mem_init(void) {
    // Configura o primeiro grande bloco representando toda a memória livre do Heap
    first_block->size = HEAP_SIZE - sizeof(struct mem_header);
    first_block->is_free = 1;
    first_block->next = 0; // NULL
}

void* kmalloc(unsigned int size) {
    // Alinhamento de memória por 4 bytes (melhora a performance da CPU de 32-bits)
    size = (size + 3) & ~3;

    struct mem_header* curr = first_block;

    // Varre a lista encadeada procurando o primeiro bloco livre que caiba o tamanho pedido (First-Fit)
    while (curr != 0) {
        if (curr->is_free && curr->size >= size) {
            
            // Se o bloco for muito maior do que o pedido, nós dividimos ele em dois (Split)
            if (curr->size >= (size + sizeof(struct mem_header) + 4)) {
                struct mem_header* new_block = (struct mem_header*)((unsigned int)curr + sizeof(struct mem_header) + size);
                new_block->size = curr->size - size - sizeof(struct mem_header);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            curr->is_free = 0; // Marca o bloco como ocupado
            
            // Retorna o ponteiro avançando os bytes do cabeçalho. 
            // O usuário recebe apenas o endereço dos dados limpos.
            return (void*)((unsigned int)curr + sizeof(struct mem_header));
        }
        curr = curr->next;
    }

    return 0; // Out of memory (Sem RAM livre)
}

void kfree(void* ptr) {
    if (ptr == 0) return;

    // Volta os bytes do cabeçalho para achar os metadados do bloco original
    struct mem_header* curr = (struct mem_header*)((unsigned int)ptr - sizeof(struct mem_header));
    curr->is_free = 1; // Libera o bloco

    // Otimização clássica de OS (Coalescing): 
    // Varre o Heap juntando blocos livres vizinhos para evitar fragmentação da memória RAM
    struct mem_header* walk = first_block;
    while (walk != 0 && walk->next != 0) {
        if (walk->is_free && walk->next->is_free) {
            walk->size += sizeof(struct mem_header) + walk->next->size;
            walk->next = walk->next->next;
            continue; // Testa novamente com o novo próximo bloco ajustado
        }
        walk = walk->next;
    }
}

unsigned int mem_get_free_space(void) {
    unsigned int free_ram = 0;
    struct mem_header* curr = first_block;
    while (curr != 0) {
        if (curr->is_free) {
            free_ram += curr->size;
        }
        curr = curr->next;
    }
    return free_ram;
}