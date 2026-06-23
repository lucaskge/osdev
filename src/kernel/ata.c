#include "ata.h"

// Importamos as funções de comunicação com portas físicas do kernel.c
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

// Lê 512 bytes do disco rígido usando o mapeamento LBA de 28 bits
void ata_read_sector(unsigned int lba, unsigned char* buffer) {
    // 1. Envia comandos de controle para o canal IDE Primário
    outb(0x1F6, (0xE0 | ((lba >> 24) & 0x0F))); // Define o modo LBA e os 4 bits mais altos do endereço
    outb(0x1F2, 1);                             // Quantidade de setores que queremos ler (1 setor = 512 bytes)
    outb(0x1F3, (unsigned char)lba);            // Bits 0-7 do LBA
    outb(0x1F4, (unsigned char)(lba >> 8));     // Bits 8-15 do LBA
    outb(0x1F5, (unsigned char)(lba >> 16));    // Bits 16-23 do LBA
    outb(0x1F7, 0x20);                          // Comando 0x20: "Read Sectors with retry"

    // 2. POLLING: Espera o disco rígido processar a leitura e ficar pronto
    while ((inb(0x1F7) & 0x80) != 0); // Espera enquanto estiver ocupado (Busy)
    while ((inb(0x1F7) & 0x08) == 0); // Espera até os dados estarem prontos (Data Request)

    // 3. TRANSFERÊNCIA: Lemos da porta de dados (0x1F0) de 2 em 2 bytes
    unsigned short* ptr = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        unsigned short data;
        __asm__ volatile ("inw %1, %0" : "=a"(data) : "Nd"(0x1F0));
        ptr[i] = data;
    }
}

// INJETADO: Grava 512 bytes no disco rígido usando mapeamento LBA de 28 bits
void ata_write_sector(unsigned int lba, unsigned char* buffer) {
    // 1. Envia comandos de controle e posicionamento para o canal IDE Primário
    outb(0x1F6, (0xE0 | ((lba >> 24) & 0x0F))); // Modo LBA e bits altos
    outb(0x1F2, 1);                             // Queremos escrever exatamente 1 setor (512 bytes)
    outb(0x1F3, (unsigned char)lba);            // Bits 0-7 do LBA
    outb(0x1F4, (unsigned char)(lba >> 8));     // Bits 8-15 do LBA
    outb(0x1F5, (unsigned char)(lba >> 16));    // Bits 16-23 do LBA
    outb(0x1F7, 0x30);                          // Comando 0x30: "Write Sectors with retry"

    // 2. POLLING: Espera o controlador do HD ficar pronto para receber os dados na RAM
    while ((inb(0x1F7) & 0x80) != 0); // Espera se estiver ocupado (Busy)
    while ((inb(0x1F7) & 0x08) == 0); // Espera até o buffer aceitar dados (Data Request pronto)

    // 3. TRANSFERÊNCIA: Descarrega o buffer de 512 bytes enviando blocos de 16 bits (short) para a porta 0x1F0
    unsigned short* ptr = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        unsigned short data = ptr[i];
        // Inline Assembly para empurrar 16 bits de uma vez de forma atômica para o hardware do HD
        __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(0x1F0));
    }
}