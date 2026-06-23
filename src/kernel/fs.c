#include "fs.h"
#include "vga.h"
#include "ata.h"

#define MAX_FILES 16

// Nossa tabela de arquivos em RAM que espelha o diretório raiz do disco
struct file_entry root_dir[MAX_FILES];
int file_count = 0;

// Buffer para ler os dados do setor do HD
unsigned char file_buffer[512];

// Função auxiliar Bare Metal para comparar nomes de arquivos
static int name_compare(const char* s1, const char* s2) {
    for (int i = 0; i < 11; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

void fs_init(void) {
    // Vamos criar dois arquivos "fakes" apontando para setores do HD para testar o sistema
    
    // 1. Arquivo de texto simples
    for(int i=0; i<11; i++) root_dir[0].name[i] = "README  TXT"[i];
    root_dir[0].attr = 0x00;    // Arquivo normal
    root_dir[0].size = 30;      // 30 bytes
    root_dir[0].cluster = 10;   // Seus dados estarão no Setor 10 do HD

    // 2. Uma pasta/diretório
    for(int i=0; i<11; i++) root_dir[1].name[i] = "DOCUMENTOS "[i];
    root_dir[1].attr = 0x10;    // Subdiretório/Pasta
    root_dir[1].size = 0;
    root_dir[1].cluster = 20;   // Começa no Setor 20

    file_count = 2;
}

void fs_list_directory(void) {
    terminal_print("Nome         Tipo          Tamanho (Bytes)\n");
    terminal_print("------------------------------------------\n");
    
    for (int i = 0; i < file_count; i++) {
        // Printa o nome
        char name_buf[12];
        for(int j=0; j<11; j++) name_buf[j] = root_dir[i].name[j];
        name_buf[11] = '\0';
        terminal_print(name_buf);
        terminal_print("  ");

        // Printa se é arquivo ou pasta
        if (root_dir[i].attr & 0x10) {
            terminal_print("<DIR>         -----\n");
        } else {
            terminal_print("<ARQ>         ");
            // Conversor rudimentar de número para string para mostrar o tamanho
            char size_str[5];
            size_str[0] = '0' + (root_dir[i].size / 10) % 10;
            size_str[1] = '0' + (root_dir[i].size % 10);
            size_str[2] = '\n';
            size_str[3] = '\0';
            terminal_print(size_str);
        }
    }
}

void fs_read_file(const char* filename) {
    for (int i = 0; i < file_count; i++) {
        if (name_compare(root_dir[i].name, filename)) {
            if (root_dir[i].attr & 0x10) {
                terminal_print("Erro: Isto e um diretorio, nao um arquivo.\n");
                return;
            }

            // Lê o setor físico do HD onde o arquivo está guardado usando o driver ATA
            ata_read_sector(root_dir[i].cluster, file_buffer);
            
            // Garante que o buffer termine em nulo para printar com segurança
            file_buffer[root_dir[i].size] = '\0';
            terminal_print((const char*)file_buffer);
            terminal_print("\n");
            return;
        }
    }
    terminal_print("Arquivo nao encontrado.\n");
}