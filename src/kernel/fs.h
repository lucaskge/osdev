#ifndef FS_H
#define FS_H

// Variável global que rastreia o cluster do diretório atual (0 = Raiz)
extern unsigned short current_directory_cluster;

void fs_init(void);
void fs_list_directory(const char* dirname); // Alterado para receber argumento
void fs_read_file(const char* filename);
void fs_create_file(const char* filename, const char* content);

// INJETADO: Assinatura para a deleção modular de arquivos
void fs_delete_file(const char* filename);

// INJETADO: Função isolada para criação de pastas
void fs_create_directory(const char[12]);

// INJETADO: Função isolada para navegação de pastas
void fs_change_directory(const char* dirname);

// INJETADO: Função de exclusão recursiva para pastas e conteúdos
void fs_delete_directory_recursive(unsigned short cluster);
#endif