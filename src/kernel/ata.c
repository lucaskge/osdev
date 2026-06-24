#include "ata.h"

extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

// Pequeno atraso de hardware padrão (400 nanossegundos) lendo o registrador de status alternativo
static void ata_io_wait(void) {
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
}

void ata_read_sector(unsigned int lba, unsigned char* buffer) {
    outb(0x1F6, (0xE0 | ((lba >> 24) & 0x0F))); 
    outb(0x1F2, 1);                             
    outb(0x1F3, (unsigned char)lba);            
    outb(0x1F4, (unsigned char)(lba >> 8));     
    outb(0x1F5, (unsigned char)(lba >> 16));    
    outb(0x1F7, 0x20); // Comando: Read Sectors                         

    while ((inb(0x1F7) & 0x80) != 0); 
    while ((inb(0x1F7) & 0x08) == 0); 

    unsigned short* ptr = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        unsigned short data;
        __asm__ volatile ("inw %1, %0" : "=a"(data) : "Nd"(0x1F0));
        ptr[i] = data;
    }
}

void ata_write_sector(unsigned int lba, unsigned char* buffer) {
    outb(0x1F6, (0xE0 | ((lba >> 24) & 0x0F))); 
    outb(0x1F2, 1);                             
    outb(0x1F3, (unsigned char)lba);            
    outb(0x1F4, (unsigned char)(lba >> 8));     
    outb(0x1F5, (unsigned char)(lba >> 16));    
    outb(0x1F7, 0x30); // Comando: Write Sectors

    // Protocolo ATA: Aguarda o Busy limpar antes de verificar se o hardware está pronto para receber
    ata_io_wait();
    while ((inb(0x1F7) & 0x80) != 0);
    while ((inb(0x1F7) & 0x08) == 0); // Agora o DRQ vai ativar pois respeitamos os ciclos de wait

    // Despeja os 512 bytes (256 words de 16 bits) no registrador de dados 0x1F0
    unsigned short* ptr = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        unsigned short data = ptr[i];
        __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(0x1F0));
    }

    // Protocolo ATA pós-escrita: Força o controlador a descarregar o cache interno para o disco
    outb(0x1F7, 0xE7); // Comando: Cache Flush
    ata_io_wait();
    while ((inb(0x1F7) & 0x80) != 0); // Aguarda a persistência física terminar
}