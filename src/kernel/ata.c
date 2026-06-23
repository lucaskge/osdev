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
    // Lê o registrador de status (Porta 0x1F7). O bit 7 (BSY) deve estar limpo
    // e o bit 3 (DRQ) deve estar setado (dados prontos para transferência).
    while ((inb(0x1F7) & 0x80) != 0); // Espera enquanto estiver ocupado (Busy)
    while ((inb(0x1F7) & 0x08) == 0); // Espera até os dados estarem prontos (Data Request)

    // 3. TRANSFERÊNCIA: O controlador do HD tem um buffer interno de 16-bits.
    // Lemos da porta de dados (0x1F0) de 2 em 2 bytes (short) até completar 256 leituras (512 bytes).
    unsigned short* ptr = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        // inline assembly para ler 16 bits de uma vez de forma rápida
        unsigned short data;
        __asm__ volatile ("inw %1, %0" : "=a"(data) : "Nd"(0x1F0));
        ptr[i] = data;
    }
}