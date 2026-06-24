#include "fs.h"
#include "vga.h"

extern void ata_read_sector(unsigned int sector, unsigned char* buffer);
extern void ata_write_sector(unsigned int sector, unsigned char* buffer);

static unsigned char root_dir_buffer[512];
static unsigned char fat_buffer[512];
static unsigned char sub_dir_buffer[512];

unsigned short current_directory_cluster = 0; 

void fs_init(void) {
    terminal_print("Sistema de Arquivos FAT com Path Tokenizer ativo.\n");
}

static unsigned int get_sector_from_cluster(unsigned short cluster) {
    if (cluster == 0) return 10;
    return 11 + (cluster - 2);
}

// Função auxiliar para extrair o próximo componente do caminho separado por '/'
// Ex: "JOGOS/MAPAS/DUST2" -> extrai "JOGOS" e avança o ponteiro para "MAPAS/DUST2"
static int get_next_token(const char* path, int start, char* token) {
    int i = start;
    // Pula barras iniciais ou duplicadas
    while (path[i] == '/') i++;
    
    if (path[i] == '\0') return -1; // Fim do caminho

    int t_idx = 0;
    while (path[i] != '/' && path[i] != '\0' && t_idx < 255) {
        token[t_idx++] = path[i++];
    }
    token[t_idx] = '\0';
    return i; // Retorna a nova posição do ponteiro na string do caminho
}

// Converte um nome normal/token em formato FAT 8.3 em letras maiúsculas alinhadas
static void format_token_to_fat(const char* input, char* output) {
    for (int i = 0; i < 11; i++) output[i] = ' ';
    output[11] = '\0';

    int i = 0;
    while (input[i] != '\0' && input[i] != '.' && i < 8) {
        if (input[i] >= 'a' && input[i] <= 'z') output[i] = input[i] - 32;
        else output[i] = input[i];
        i++;
    }
    while (input[i] != '\0' && input[i] != '.') i++;
    if (input[i] == '.') {
        i++;
        int ext_idx = 0;
        while (input[i] != '\0' && ext_idx < 3) {
            if (input[i] >= 'a' && input[i] <= 'z') output[8 + ext_idx] = input[i] - 32;
            else output[8 + ext_idx] = input[i];
            i++;
            ext_idx++;
        }
    }
}

// RESOLVEDOR DE CAMINHOS CRÍTICO:
// Navega pelo HD seguindo as barras e retorna o cluster do diretório PAI do alvo final.
// Também guarda em 'final_fat_name' o nome formatado do último elemento (arquivo ou pasta).
static unsigned short fs_resolve_path(const char* path, char* final_fat_name) {
    unsigned short walk_cluster = current_directory_cluster;
    
    // Se o caminho começar com '/', a busca se torna Absoluta a partir da raiz
    int path_idx = 0;
    if (path[0] == '/') {
        walk_cluster = 0;
        path_idx = 1;
    }

    char current_token[256];
    char next_token[256];
    
    int next_idx = get_next_token(path, path_idx, current_token);
    if (next_idx == -1) {
        // Se o caminho for vazio ou apenas "/", refere-se à raiz
        format_token_to_fat("", final_fat_name);
        return 0;
    }

    // Enquanto houver mais subníveis pela frente no caminho
    while (1) {
        int check_next = get_next_token(path, next_idx, next_token);
        
        // Se 'next_token' estiver vazio, significa que 'current_token' é o ALVO FINAL do comando!
        if (check_next == -1) {
            format_token_to_fat(current_token, final_fat_name);
            return walk_cluster;
        }

        // Caso contrário, precisamos entrar na pasta representada por 'current_token'
        char fat_search_name[12];
        format_token_to_fat(current_token, fat_search_name);
        
        unsigned int current_sector = get_sector_from_cluster(walk_cluster);
        ata_read_sector(current_sector, sub_dir_buffer);

        int found = 0;
        for (int i = 0; i < 16; i++) {
            int offset = i * 32;
            if (sub_dir_buffer[offset] == 0x00) break;
            if (sub_dir_buffer[offset] == 0xE5) continue;

            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (sub_dir_buffer[offset + j] != fat_search_name[j]) {
                    match = 0;
                    break;
                }
            }

            if (match && (sub_dir_buffer[offset + 11] == 0x10)) {
                walk_cluster = *(unsigned short*)&sub_dir_buffer[offset + 26];
                found = 1;
                break;
            }
        }

        if (!found) {
            // Caminho quebrado no meio da árvore
            final_fat_name[0] = '\0';
            return 0xFFFF;
        }

        // Avança para o próximo componente extraído
        next_idx = next_idx; 
        int c = 0;
        while(next_token[c] != '\0') {
            current_token[c] = next_token[c];
            c++;
        }
        current_token[c] = '\0';
        next_idx = check_next;
    }
}

static int fs_exists_in_sector(unsigned int sector, const char* fat_name) {
    unsigned char check_buffer[512];
    ata_read_sector(sector, check_buffer);
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (check_buffer[offset] == 0x00) break;
        if (check_buffer[offset] == 0xE5) continue;
        if (check_buffer[offset] == '.') continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (check_buffer[offset + j] != fat_name[j]) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

void fs_list_directory(const char* path) {
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Caminho de diretorio invalido.\n");
        return;
    }

    unsigned short list_cluster = parent_cluster;

    // Se o usuário digitou um caminho completo para listar uma pasta (ex: "ls /jogos/mapas")
    // Precisamos achar o cluster da própria pasta final indicada
    if (target_name[0] != ' ' || target_name[1] != ' ') {
        unsigned int sector = get_sector_from_cluster(parent_cluster);
        ata_read_sector(sector, root_dir_buffer);
        int found = 0;

        for (int i = 0; i < 16; i++) {
            int offset = i * 32;
            if (root_dir_buffer[offset] == 0x00) break;
            if (root_dir_buffer[offset] == 0xE5) continue;

            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (root_dir_buffer[offset + j] != target_name[j]) {
                    match = 0;
                    break;
                }
            }

            if (match && root_dir_buffer[offset + 11] == 0x10) {
                list_cluster = *(unsigned short*)&root_dir_buffer[offset + 26];
                found = 1;
                break;
            }
        }

        if (!found) {
            terminal_print("Erro: Pasta nao encontrada.\n");
            return;
        }
    }

    unsigned int target_sector = get_sector_from_cluster(list_cluster);
    ata_read_sector(target_sector, root_dir_buffer);

    terminal_print("Listando Diretorio:\n");
    int arquivos_encontrados = 0;
    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue;
        if (root_dir_buffer[offset] == '.') continue;

        arquivos_encontrados++;
        if (root_dir_buffer[offset + 11] == 0x10) terminal_print("  [DIR] ");
        else terminal_print("  [ARQ] ");

        for (int j = 0; j < 8; j++) {
            if (root_dir_buffer[offset + j] != ' ') terminal_putchar(root_dir_buffer[offset + j]);
        }
        if (root_dir_buffer[offset + 11] != 0x10) {
            terminal_print(".");
            for (int j = 0; j < 3; j++) terminal_putchar(root_dir_buffer[offset + 8 + j]);
        }
        terminal_print("\n");
    }

    if (arquivos_encontrados == 0) terminal_print("  (Diretorio vazio)\n");
}

void fs_read_file(const char* path) {
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Caminho invalido.\n");
        return;
    }

    unsigned int sector = get_sector_from_cluster(parent_cluster);
    ata_read_sector(sector, root_dir_buffer);

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (root_dir_buffer[offset + j] != target_name[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            if (root_dir_buffer[offset + 11] == 0x10) {
                terminal_print("Erro: Alvo e uma pasta.\n");
                return;
            }

            unsigned char data_buffer[512];
            unsigned short cluster = *(unsigned short*)&root_dir_buffer[offset + 26];
            unsigned int target_sector = get_sector_from_cluster(cluster);
            
            ata_read_sector(target_sector, data_buffer);
            terminal_print("Conteudo:\n\"");
            terminal_print((char*)data_buffer);
            terminal_print("\"\n");
            return;
        }
    }
    terminal_print("Erro: Arquivo nao encontrado.\n");
}

void fs_create_file(const char* path, const char* content) {
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Caminho destino invalido.\n");
        return;
    }

    unsigned int parent_sector = get_sector_from_cluster(parent_cluster);

    if (fs_exists_in_sector(parent_sector, target_name)) {
        terminal_print("Erro: Nome ja existente neste diretorio.\n");
        return;
    }

    ata_read_sector(9, fat_buffer);
    int free_cluster = -1;
    unsigned short* fat_table = (unsigned short*)fat_buffer;
    for (int c = 2; c < 256; c++) {
        if (fat_table[c] == 0x0000) {
            free_cluster = c;
            break;
        }
    }

    if (free_cluster == -1) {
        terminal_print("Erro: FAT cheia.\n");
        return;
    }

    ata_read_sector(parent_sector, root_dir_buffer);
    int entry_offset = -1;
    int start_loop = (parent_cluster == 0) ? 0 : 2;

    for (int i = start_loop; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00 || root_dir_buffer[offset] == 0xE5) {
            entry_offset = offset;
            break;
        }
    }

    if (entry_offset == -1) {
        terminal_print("Erro: Diretorio cheio.\n");
        return;
    }

    fat_table[free_cluster] = 0xFFFF;
    ata_write_sector(9, fat_buffer);

    for (int j = 0; j < 11; j++) root_dir_buffer[entry_offset + j] = target_name[j];
    root_dir_buffer[entry_offset + 11] = 0x20; 

    *(unsigned short*)&root_dir_buffer[entry_offset + 26] = free_cluster;
    *(unsigned int*)&root_dir_buffer[entry_offset + 28] = 512;
    ata_write_sector(parent_sector, root_dir_buffer);

    unsigned char data_sector[512];
    for (int k = 0; k < 512; k++) data_sector[k] = '\0';
    int len = 0;
    while (content[len] != '\0' && len < 511) {
        data_sector[len] = content[len];
        len++;
    }
    data_sector[len] = '\0';

    unsigned int target_sector = get_sector_from_cluster(free_cluster);
    ata_write_sector(target_sector, data_sector);
    terminal_print("Arquivo criado com sucesso.\n");
}

void fs_create_directory(const char* path) {
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Caminho pai inexistente.\n");
        return;
    }

    unsigned int parent_sector = get_sector_from_cluster(parent_cluster);

    if (fs_exists_in_sector(parent_sector, target_name)) {
        terminal_print("Erro: Pasta ja existe.\n");
        return;
    }

    ata_read_sector(9, fat_buffer);
    int free_cluster = -1;
    unsigned short* fat_table = (unsigned short*)fat_buffer;
    for (int c = 2; c < 256; c++) {
        if (fat_table[c] == 0x0000) {
            free_cluster = c;
            break;
        }
    }

    if (free_cluster == -1) {
        terminal_print("Erro: FAT cheia.\n");
        return;
    }

    ata_read_sector(parent_sector, root_dir_buffer);
    int entry_offset = -1;
    int start_loop = (parent_cluster == 0) ? 0 : 2;

    for (int i = start_loop; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00 || root_dir_buffer[offset] == 0xE5) {
            entry_offset = offset;
            break;
        }
    }

    if (entry_offset == -1) {
        terminal_print("Erro: Diretorio cheio.\n");
        return;
    }

    fat_table[free_cluster] = 0xFFFF;
    ata_write_sector(9, fat_buffer);

    for (int j = 0; j < 11; j++) root_dir_buffer[entry_offset + j] = target_name[j];
    root_dir_buffer[entry_offset + 11] = 0x10; 

    *(unsigned short*)&root_dir_buffer[entry_offset + 26] = free_cluster;
    *(unsigned int*)&root_dir_buffer[entry_offset + 28] = 0;
    ata_write_sector(parent_sector, root_dir_buffer);

    unsigned char empty_sector[512];
    for (int k = 0; k < 512; k++) empty_sector[k] = 0x00;
    empty_sector[0] = '.';
    for(int j=1; j<11; j++) empty_sector[j] = ' ';
    empty_sector[11] = 0x10;
    *(unsigned short*)&empty_sector[26] = free_cluster;

    empty_sector[32] = '.'; empty_sector[33] = '.';
    for(int j=34; j<43; j++) empty_sector[j] = ' ';
    empty_sector[43] = 0x10;
    *(unsigned short*)&empty_sector[58] = parent_cluster;

    unsigned int target_sector = get_sector_from_cluster(free_cluster);
    ata_write_sector(target_sector, empty_sector);
    terminal_print("Pasta criada com sucesso.\n");
}

void fs_delete_file(const char* path) {
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Caminho invalido.\n");
        return;
    }

    unsigned int parent_sector = get_sector_from_cluster(parent_cluster);
    ata_read_sector(parent_sector, root_dir_buffer);

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (root_dir_buffer[offset + j] != target_name[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            unsigned short cluster = *(unsigned short*)&root_dir_buffer[offset + 26];

            if (root_dir_buffer[offset + 11] == 0x10) {
                terminal_print("Aviso: Remocao recursiva de subpasta...\n");
                fs_delete_directory_recursive(cluster);
                
                ata_read_sector(parent_sector, root_dir_buffer);
                root_dir_buffer[offset] = 0xE5; 
                ata_write_sector(parent_sector, root_dir_buffer);
                terminal_print("Pasta e subconteudos pulverizados!\n");
                return;
            }

            ata_read_sector(9, fat_buffer);
            unsigned short* fat_table = (unsigned short*)fat_buffer;
            fat_table[cluster] = 0x0000;
            ata_write_sector(9, fat_buffer);

            root_dir_buffer[offset] = 0xE5; 
            ata_write_sector(parent_sector, root_dir_buffer);
            terminal_print("Arquivo removido com sucesso!\n");
            return;
        }
    }
    terminal_print("Erro: Alvo nao encontrado.\n");
}

void fs_change_directory(const char* path) {
    // 1. TRATAMENTO DE RETORNO À RAIZ ABSOLUTA (cd / ou cd vazio)
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        current_directory_cluster = 0;
        terminal_print("Voltando para a raiz [/].\n");
        return;
    }

    // 2. TRATAMENTO CRÍTICO E ROBUSTO PARA VOLTAR NÍVEL (cd .. ou cd ../)
    // Se os dois primeiros caracteres forem '..', interceptamos antes do resolvedor de caminhos
    if (path[0] == '.' && path[1] == '.') {
        if (current_directory_cluster == 0) {
            terminal_print("Voce ja esta na raiz [/].\n");
            return;
        }
        
        // Lê o setor da subpasta atual para pegar o cluster do pai
        unsigned int current_sector = get_sector_from_cluster(current_directory_cluster);
        ata_read_sector(current_sector, root_dir_buffer);
        
        // A entrada '..' fica cravada fixamente no offset 32 (segunda entrada) do setor da subpasta
        current_directory_cluster = *(unsigned short*)&root_dir_buffer[32 + 26];
        terminal_print("Voltando um nivel.\n");
        return;
    }

    // 3. NAVEGAÇÃO PARA FRENTE (Caminhos normais ou longos)
    char target_name[12];
    unsigned short parent_cluster = fs_resolve_path(path, target_name);

    if (parent_cluster == 0xFFFF) {
        terminal_print("Erro: Diretorio invalido.\n");
        return;
    }

    // Se resolveu o caminho e sobrou apenas espaços, já mudou o cluster internamente
    if (target_name[0] == ' ' && target_name[1] == ' ') {
        current_directory_cluster = parent_cluster;
        terminal_print("Diretorio alterado.\n");
        return;
    }

    unsigned int sector = get_sector_from_cluster(parent_cluster);
    ata_read_sector(sector, root_dir_buffer);

    for (int i = 0; i < 16; i++) {
        int offset = i * 32;
        if (root_dir_buffer[offset] == 0x00) break;
        if (root_dir_buffer[offset] == 0xE5) continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            if (root_dir_buffer[offset + j] != target_name[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            if (root_dir_buffer[offset + 11] == 0x10) {
                current_directory_cluster = *(unsigned short*)&root_dir_buffer[offset + 26];
                terminal_print("Entrando no diretorio.\n");
                return;
            } else {
                terminal_print("Erro: Alvo nao e uma pasta.\n");
                return;
            }
        }
    }
    terminal_print("Erro: Diretorio nao encontrado.\n");
}

void fs_delete_directory_recursive(unsigned short cluster) {
    if (cluster == 0) return; 
    unsigned char local_buffer[512];
    unsigned int sector = 11 + (cluster - 2);
    ata_read_sector(sector, local_buffer);

    for (int i = 2; i < 16; i++) {
        int offset = i * 32;
        if (local_buffer[offset] == 0x00) break; 
        if (local_buffer[offset] == 0xE5) continue; 

        unsigned short child_cluster = *(unsigned short*)&local_buffer[offset + 26];
        if (local_buffer[offset + 11] == 0x10) {
            fs_delete_directory_recursive(child_cluster);
        } else {
            ata_read_sector(9, fat_buffer);
            unsigned short* fat_table = (unsigned short*)fat_buffer;
            fat_table[child_cluster] = 0x0000;
            ata_write_sector(9, fat_buffer);
        }
    }
    ata_read_sector(9, fat_buffer);
    unsigned short* fat_table = (unsigned short*)fat_buffer;
    fat_table[cluster] = 0x0000;
    ata_write_sector(9, fat_buffer);
}