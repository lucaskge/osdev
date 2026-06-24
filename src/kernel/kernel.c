#include "vga.h"
#include "shell.h"
#include "ata.h"
#include "fs.h" // Inclui o header do FS
#include "mem.h"

void idt_init(void);
void pic_init(void);
void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags);

unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));

void paging_init(void) {
    for(int i = 0; i < 1024; i++) page_directory[i] = 0x00000002;
    for(unsigned int i = 0; i < 1024; i++) {
        unsigned int address = i * 4096;
        first_page_table[i] = address | 3;
    }
    page_directory[0] = ((unsigned int)&first_page_table) | 3;
    __asm__ volatile("mov %0, %%cr3" : : "r"(&page_directory));
    unsigned int cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// --- IDT ---
struct idt_entry {
    unsigned short offset_lowerbits; unsigned short selector;
    unsigned char zero; unsigned char type_attr; unsigned short offset_higherbits;
} __attribute__((packed));
struct idt_ptr { unsigned short limit; unsigned int base; } __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].offset_lowerbits = (base & 0xFFFF);
    idt[num].offset_higherbits = (base >> 16) & 0xFFFF;
    idt[num].selector = sel; idt[num].zero = 0; idt[num].type_attr = flags;
}
void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    extern void isr_ignore(void);
    for(int i = 0; i < 256; i++) idt_set_gate(i, (unsigned long)isr_ignore, 0x08, 0x8E);
}
void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02); outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFD); outb(0xA1, 0xFF);
}
// void exception0_handler(void) {
//     terminal_print("\n[CRITICAL] Divisao por zero!\n");
//     while(1) { __asm__ volatile("hlt"); }
// }

unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};
void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);
    if (!(scancode & 0x80)) {
        char letra = kbd_us[scancode];
        if (letra != 0) shell_input_char(letra);
    }
}

void kernel_main(void) {
    mem_init();  // Ativa o alocador dinâmico antes de tudo

    terminal_clear();
    terminal_print("MeuOS Bare Metal - Inicializando Tabelas e Sistema de Arquivos...\n");
    
    idt_init();                        
    pic_init();                        

    extern void isr0(void);
    extern void isr33(void);
    extern void isr6(void);   // Adicionado
    extern void isr13(void);  // Adicionado
    extern void isr14(void);  // Adicionado

    // Mapeia os tratamentos na tabela de vetores da CPU
    idt_set_gate(0, (unsigned long)isr0, 0x08, 0x8E);   // Divisão por zero
    idt_set_gate(6, (unsigned long)isr6, 0x08, 0x8E);   // Opcode inválido
    idt_set_gate(13, (unsigned long)isr13, 0x08, 0x8E); // General Protection Fault
    idt_set_gate(14, (unsigned long)isr14, 0x08, 0x8E); // Page Fault
    idt_set_gate(33, (unsigned long)isr33, 0x08, 0x8E); // Teclado

    // Removido função 'exception0_handler' antiga que estava solta no kernel.c, 
    // pois agora ela foi absorvida de forma muito mais genérica pelo 'exception_handler' do exception.c

    paging_init();
    
    // 1. Inicializa a tabela de mapeamento FAT interna
    fs_init();

    terminal_print("Sistema de Arquivos FAT estruturado na RAM.\n");
    shell_print_prompt();
}