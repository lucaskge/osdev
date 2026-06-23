#include "fs.h"
#include "vga.h"

// Importamos as funções físicas de leitura e escrita do driver ATA (ata.c)
extern void ata_read_sector(unsigned int sector, unsigned char* buffer);
extern void ata_write_sector(unsigned int sector, unsigned char* buffer);

// Buffer temporário de 512 bytes para manipulação de setores na RAM
static unsigned char fs_buffer[512];

void fs_init(void) {
    terminal_print("Sistema de Arquivos FAT inicializado no Kernel.\n");
}

void fs_list_directory(void) {
    ata_read_sector(10, fs_buffer);

    terminal_print("Arquivos no Diretorio Raiz:\n");
    
    int arquivos_encontrados = 0;
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        
        if (fs_buffer[offset] == 0x00) {
            break;
        }
        
        if (fs_buffer[offset] == 0xE5) {
            continue;
        }

        arquivos_encontrados++;
        terminal_print("  - ");
        
        for (int j = 0; j < 8; j++) {
            if (fs_buffer[offset + j] != ' ') {
                terminal_putchar(fs_buffer[offset + j]);
            }
        }
        
        terminal_print(".");
        
        for (int j = 0; j < 3; j++) {
            terminal_putchar(fs_buffer[offset + 8 + j]);
        }
        
        terminal_print("\n");
    }

    if (arquivos_encontrados == 0) {
        terminal_print("  (Diretorio vazio - Nenhum arquivo encontrado)\n");
    }
}

void fs_read_file(const char* filename) {
    // Carrega o setor 10 (Diretório Raiz) para a RAM
    ata_read_sector(10, fs_buffer);

    // Varre as 16 entradas possíveis de 32 bytes cada
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (fs_buffer[offset] == 0x00) break;
        if (fs_buffer[offset] == 0xE5) continue;

        // Compara estritamente os 11 bytes do nome + extensão do padrão FAT
        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (fs_buffer[offset + j] != filename[j]) {
                match = 0;
                break;
            }
        }

        // Se encontrou o arquivo correspondente
        if (match) {
            unsigned char data_buffer[512];
            
            // Lê o cluster inicial (offset 26 da entrada de diretório)
            unsigned short cluster = *(unsigned short*)&fs_buffer[offset + 26];
            
            // Mapeamento de setores baseado no cluster do arquivo
            if (cluster == 2) {
                ata_read_sector(11, data_buffer); // Nosso arquivo criado via echo fica no setor 11
            } else {
                ata_read_sector(10, data_buffer); // Fallback para o setor do diretório/readme antigo
            }
            
            terminal_print("Conteudo do arquivo:\n\"");
            terminal_print((char*)data_buffer);
            terminal_print("\"\n");
            return;
        }
    }
    
    // Debug visual para você ver exatamente o que o Kernel tentou buscar na RAM
    terminal_print("Erro: Arquivo nao encontrado [");
    for(int x = 0; x < 11; x++) {
        if(filename[x] == ' ') terminal_putchar('_'); // Mostra espaços como underscores no log de erro
        else terminal_putchar(filename[x]);
    }
    terminal_print("]\n");
}

void fs_create_file(const char* filename, const char* content) {
    ata_read_sector(10, fs_buffer);

    int entry_offset = -1;

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (fs_buffer[offset] == 0x00 || fs_buffer[offset] == 0xE5) {
            entry_offset = offset;
            break;
        }
    }

    if (entry_offset == -1) {
        terminal_print("Erro: Diretorio raiz cheio.\n");
        return;
    }

    for (int j = 0; j < 11; j++) {
        fs_buffer[entry_offset + j] = filename[j];
    }

    fs_buffer[entry_offset + 11] = 0x20; 

    unsigned short* cluster_ptr = (unsigned short*)&fs_buffer[entry_offset + 26];
    *cluster_ptr = 2;

    unsigned int* size_ptr = (unsigned int*)&fs_buffer[entry_offset + 28];
    *size_ptr = 512;

    ata_write_sector(10, fs_buffer);

    unsigned char data_sector[512];
    for (int k = 0; k < 512; k++) {
        data_sector[k] = '\0';
    }

    int len = 0;
    while (content[len] != '\0' && len < 511) {
        data_sector[len] = content[len];
        len++;
    }
    data_sector[len] = '\0';

    ata_write_sector(11, data_sector);

    terminal_print("Arquivo criado e persistido no HD com sucesso!\n");
}