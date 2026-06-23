#ifndef FS_H
#define FS_H

// Estrutura que representa um arquivo ou diretório na tabela do sistema
struct file_entry {
    char name[12];          // Nome do arquivo (formato 8.3, ex: "TESTE   TXT")
    unsigned char attr;     // Atributos (0x10 para pasta/diretório, 0x00 para arquivo)
    unsigned int size;      // Tamanho do arquivo em bytes
    unsigned int cluster;   // Setor/Cluster onde os dados começam no HD
};

// Inicializa o sistema de arquivos simulando entradas na raiz
void fs_init(void);

// Lista os arquivos e pastas na tela (para o comando 'ls')
void fs_list_directory(void);

// Exibe o conteúdo de um arquivo na tela (para o comando 'cat')
void fs_read_file(const char* filename);

#endif