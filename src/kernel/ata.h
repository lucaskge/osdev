#ifndef ATA_H
#define ATA_H

// Lê um setor específico (LBA28) do disco rígido e joga os 512 bytes no buffer
void ata_read_sector(unsigned int lba, unsigned char* buffer);

#endif