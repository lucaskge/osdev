#include "fs.h"
#include "vga.h"

extern void ata_read_sector(unsigned int sector, unsigned char* buffer);
extern void ata_write_sector(unsigned int sector, unsigned char* buffer);

static unsigned char root_dir_buffer[512];
static unsigned char fat_buffer[512];

void fs_init(void) {
    terminal_print("Sistema de Arquivos FAT inicializado.\n");
}

void fs_list_directory(void) {
    ata_read_sector(10, root_dir_buffer);

    terminal_print("Arquivos no Diretorio Raiz:\n");
    
    int arquivos_encontrados = 0;
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        
        if (root_dir_buffer[offset] == 0x00) {
            break;
        }
        
        if (root_dir_buffer[offset] == 0xE5) {
            continue;
        }

        arquivos_encontrados++;
        terminal_print("  - ");
        
        for (int j = 0; j < 8; j++) {
            if (root_dir_buffer[offset + j] != ' ') {
                terminal_putchar(root_dir_buffer[offset + j]);
            }
        }
        
        terminal_print(".");
        
        for (int j = 0; j < 3; j++) {
            terminal_putchar(root_dir_buffer[offset + 8 + j]);
        }
        
        terminal_print("\n");
    }

    if (arquivos_encontrados == 0) {
        terminal_print("  (Diretorio vazio)\n");
    }
}

void fs_read_file(const char* filename) {
    ata_read_sector(10, root_dir_buffer);

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (root_dir_buffer[offset + j] != filename[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            unsigned char data_buffer[512];
            unsigned short cluster = *(unsigned short*)&root_dir_buffer[offset + 26];
            unsigned int target_sector = 11 + (cluster - 2);
            
            ata_read_sector(target_sector, data_buffer);
            
            terminal_print("Conteudo do arquivo:\n\"");
            terminal_print((char*)data_buffer);
            terminal_print("\"\n");
            return;
        }
    }
    terminal_print("Erro: Arquivo nao encontrado.\n");
}

void fs_create_file(const char* filename, const char* content) {
    // Leitura preventiva dos setores para garantir sincronia com a RAM
    ata_read_sector(9, fat_buffer);
    
    int free_cluster = -1;
    unsigned short* fat_table = (unsigned short*)fat_buffer;

    // Procurando cluster livre
    for (int c = 2; c < 256; c++) {
        if (fat_table[c] == 0x0000) {
            free_cluster = c;
            break;
        }
    }

    if (free_cluster == -1) {
        terminal_print("Erro: Sem espaco na tabela FAT.\n");
        return;
    }

    ata_read_sector(10, root_dir_buffer);
    int entry_offset = -1;

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00 || root_dir_buffer[offset] == 0xE5) {
            entry_offset = offset;
            break;
        }
    }

    if (entry_offset == -1) {
        terminal_print("Erro: Diretorio raiz cheio.\n");
        return;
    }

    // Atualiza estruturas na RAM primeiro para evitar escrita parcial se o hardware falhar
    fat_table[free_cluster] = 0xFFFF;

    for (int j = 0; j < 11; j++) {
        root_dir_buffer[entry_offset + j] = filename[j];
    }
    root_dir_buffer[entry_offset + 11] = 0x20; 

    unsigned short* cluster_ptr = (unsigned short*)&root_dir_buffer[entry_offset + 26];
    *cluster_ptr = free_cluster;

    unsigned int* size_ptr = (unsigned int*)&root_dir_buffer[entry_offset + 28];
    *size_ptr = 512;

    unsigned char data_sector[512];
    for (int k = 0; k < 512; k++) data_sector[k] = '\0';

    int len = 0;
    while (content[len] != '\0' && len < 511) {
        data_sector[len] = content[len];
        len++;
    }
    data_sector[len] = '\0';

    unsigned int target_sector = 11 + (free_cluster - 2);

    // Despeja os blocos sequencialmente no driver ATA
    terminal_print("Modificando setores no HD virtual...\n");
    
    ata_write_sector(9, fat_buffer);
    ata_write_sector(10, root_dir_buffer);
    ata_write_sector(target_sector, data_sector);

    terminal_print("Sucesso! Criado no Cluster ");
    char c_buf[4];
    c_buf[0] = '0' + (free_cluster / 10);
    c_buf[1] = '0' + (free_cluster % 10);
    c_buf[2] = '\0';
    terminal_print(c_buf);
    terminal_print("\n");
}

void fs_delete_file(const char* filename) {
    // Carrega o Diretório Raiz (Setor 10) para a RAM
    ata_read_sector(10, root_dir_buffer);

    // Varre as entradas para encontrar o arquivo correspondente
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue; // Pula os já deletados

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (root_dir_buffer[offset + j] != filename[j]) {
                match = 0;
                break;
            }
        }

        // Se encontrou o arquivo que bate exatamente com o nome e extensão
        if (match) {
            // Captura o número do cluster inicial que o arquivo estava usando (bytes 26-27)
            unsigned short cluster = *(unsigned short*)&root_dir_buffer[offset + 26];

            // 1. LIMPEZA NA TABELA FAT (Setor 9)
            ata_read_sector(9, fat_buffer);
            unsigned short* fat_table = (unsigned short*)fat_buffer;
            
            // Define o cluster como 0x0000 (Livre para novas gravações do echo)
            fat_table[cluster] = 0x0000;
            ata_write_sector(9, fat_buffer);

            // 2. MARCAÇÃO DE DELEÇÃO NO DIRETÓRIO RAIZ (Setor 10)
            // O byte mágico 0xE5 indica que o arquivo foi excluído, liberando a entrada de 32 bytes
            root_dir_buffer[offset] = 0xE5; 
            ata_write_sector(10, root_dir_buffer);

            terminal_print("Arquivo removido do sistema com sucesso!\n");
            return;
        }
    }

    terminal_print("Erro: Arquivo nao encontrado para exclusao.\n");
}