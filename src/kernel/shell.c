#include "shell.h"
#include "vga.h"
#include "fs.h"
#include "rtc.h"
#include "task.h"
#include "mem.h"

#define BUFFER_SIZE 256

char shell_buffer[BUFFER_SIZE];
int buffer_idx = 0;

extern void outb(unsigned short port, unsigned char val);
extern void run_scheduler(void);

int str_prefix_compare(const char* buffer, const char* cmd) {
    int i = 0;
    while (cmd[i] != '\0') {
        if (buffer[i] != cmd[i]) return 0;
        i++;
    }
    return (buffer[i] == '\0' || buffer[i] == ' ');
}

void shell_print_prompt(void) {
    if (current_directory_cluster == 0) {
        terminal_print("\n[/] > ");
    } else {
        terminal_print("\n[sub] > ");
    }
}

void sys_shutdown(void) {
    terminal_print("Desligando o sistema...\n");
    outb(0xB004, 0x2000);
    __asm__ volatile ("outw %0, %1" : : "a"((unsigned short)0x2000), "Nd"((unsigned short)0x604));
    while(1) { __asm__ volatile("hlt"); }
}

void shell_print_num2(int num) {
    char buf[3];
    buf[0] = '0' + (num / 10);
    buf[1] = '0' + (num % 10);
    buf[2] = '\0';
    terminal_print(buf);
}

// Mantida no arquivo para compatibilidade, caso outros módulos necessitem
void format_fat_name(const char* input, char* output) {
    for (int i = 0; i < 11; i++) output[i] = ' ';
    output[11] = '\0';

    int i = 0;
    while (input[i] != '\0' && input[i] != '.' && input[i] != ' ' && i < 8) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            output[i] = input[i] - 32;
        } else {
            output[i] = input[i];
        }
        i++;
    }

    while (input[i] != '\0' && input[i] != '.') {
        if (input[i] == ' ') return; 
        i++;
    }

    if (input[i] == '.') {
        i++; 
        int ext_idx = 0;
        while (input[i] != '\0' && input[i] != ' ' && ext_idx < 3) {
            if (input[i] >= 'a' && input[i] <= 'z') {
                output[8 + ext_idx] = input[i] - 32;
            } else {
                output[8 + ext_idx] = input[i];
            }
            i++;
            ext_idx++;
        }
    }
}

void shell_execute_command(void) {
    if (buffer_idx == 0) {
        shell_print_prompt();
        return;
    }

    shell_buffer[buffer_idx] = '\0';
    terminal_print("\n");

    if (str_prefix_compare(shell_buffer, "help")) {
        terminal_print("Comandos disponiveis:\n");
        terminal_print("  help                       - Mostra esta lista de comandos\n");
        terminal_print("  clear                      - Limpa a tela do terminal\n");
        terminal_print("  about                      - Exibe informacoes do sistema\n");
        terminal_print("  ls [caminho]               - Lista arquivos do diretorio atual ou indicado\n");
        terminal_print("  cat [caminho_arquivo]      - Exibe o conteudo de um arquivo multinivel\n");
        terminal_print("  echo [caminho.ext] [texto] - Cria um arquivo em qualquer subpasta\n");
        terminal_print("  time                       - Exibe a data e hora atual do chip RTC\n");
        terminal_print("  mem                        - Exibe o espaco livre de memoria RAM do Heap\n");
        terminal_print("  task                       - Inicia o agendador de multitarefa paralela\n");
        terminal_print("  shutdown                   - Desliga a maquina virtual\n");
        terminal_print("  rm [caminho]               - Remove arquivo ou pasta recursivamente\n");
        terminal_print("  mkdir [caminho_pasta]      - Cria uma nova pasta (suporta subniveis)\n");
        terminal_print("  cd [caminho_pasta]         - Altera o diretorio de trabalho de forma relativa ou absoluta\n");
    } 
    else if (str_prefix_compare(shell_buffer, "clear")) {
        terminal_clear();
    } 
    else if (str_prefix_compare(shell_buffer, "about")) {
        terminal_print("MeuOS v1.0.0 Bare Metal x86_32\n");
        terminal_print("Interpretador de comandos com suporte a Path Tokenizer hierarquico.");
    } 
    else if (str_prefix_compare(shell_buffer, "ls")) {
        int arg_idx = 2;
        while (shell_buffer[arg_idx] == ' ') arg_idx++;

        // Repassa o argumento diretamente para o resolvedor de caminhos
        fs_list_directory(&shell_buffer[arg_idx]);
    }
    else if (str_prefix_compare(shell_buffer, "cat")) {
        int arg_idx = 3;
        while (shell_buffer[arg_idx] == ' ') arg_idx++;

        if (shell_buffer[arg_idx] == '\0') {
            terminal_print("Uso: cat [CAMINHO_DO_ARQUIVO.EXT]\n");
        } else {
            // Envia o caminho multinível completo sem truncar
            fs_read_file(&shell_buffer[arg_idx]);
        }
    }
    else if (str_prefix_compare(shell_buffer, "rm")) {
        int arg_idx = 2;
        while (shell_buffer[arg_idx] == ' ') arg_idx++;

        if (shell_buffer[arg_idx] == '\0') {
            terminal_print("Uso: rm [CAMINHO_ALVO]\n");
        } else {
            // Aciona a exclusão inteligivel (arquivo ou árvore recursiva de diretórios)
            fs_delete_file(&shell_buffer[arg_idx]);
        }
    }
    else if (str_prefix_compare(shell_buffer, "echo")) {
        int idx = 4;
        while (shell_buffer[idx] == ' ') idx++;

        if (shell_buffer[idx] == '\0') {
            terminal_print("Uso: echo [CAMINHO_DO_ARQUIVO.EXT] [MENSAGEM DE TEXTO]\n");
        } else {
            const char* file_arg = &shell_buffer[idx];

            while (shell_buffer[idx] != ' ' && shell_buffer[idx] != '\0') {
                idx++;
            }

            if (shell_buffer[idx] == '\0') {
                terminal_print("Erro: Digite o conteudo do texto apos o nome do arquivo.\n");
            } else {
                shell_buffer[idx] = '\0'; // Separa dinamicamente o path longo do texto
                idx++;
                while (shell_buffer[idx] == ' ') idx++;

                if (shell_buffer[idx] == '\0') {
                    terminal_print("Erro: Arquivo nao pode ser criado vazio.\n");
                } else {
                    const char* content_arg = &shell_buffer[idx];
                    // Dispara a criação no diretório alvo do caminho indicado
                    fs_create_file(file_arg, content_arg);
                }
            }
        }
    }
    else if (str_prefix_compare(shell_buffer, "time")) {
        struct rtc_time current_time;
        rtc_get_time(&current_time);
        terminal_print("Data/Hora Atual: ");
        shell_print_num2(current_time.day); terminal_print("/");
        shell_print_num2(current_time.month); terminal_print("/");
        char year_buf[5];
        year_buf[0] = '0' + (current_time.year / 1000);
        year_buf[1] = '0' + ((current_time.year / 100) % 10);
        year_buf[2] = '0' + ((current_time.year / 10) % 10);
        year_buf[3] = '0' + (current_time.year % 10); year_buf[4] = '\0';
        terminal_print(year_buf); terminal_print(" ");
        shell_print_num2(current_time.hour); terminal_print(":");
        shell_print_num2(current_time.minute); terminal_print(":");
        shell_print_num2(current_time.second);
    }
    else if (str_prefix_compare(shell_buffer, "task")) {
        terminal_print("Passando o controle para os lacos paralelos...\n");
        task_init();       
        start_multitasking();  
    }
    else if (str_prefix_compare(shell_buffer, "shutdown")) {
        sys_shutdown();
    }
    else if (str_prefix_compare(shell_buffer, "mem")) {
        unsigned int free_bytes = mem_get_free_space();
        
        terminal_print("Estatísticas de Memória RAM:\n");
        terminal_print("  Heap Livre: ");
        
        // Conversão simples de inteiro para string para exibir os bytes na tela
        char num_buf[32];
        int i = 0;
        if (free_bytes == 0) {
            terminal_print("0");
        } else {
            unsigned int temp = free_bytes;
            while (temp > 0 && i < 30) {
                num_buf[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            num_buf[i] = '\0';
            // Imprime invertido
            for (int j = i - 1; j >= 0; j--) {
                terminal_putchar(num_buf[j]);
            }
        }
        terminal_print(" bytes.\n");
    }
    else if (str_prefix_compare(shell_buffer, "mkdir")) {
        int arg_idx = 5;
        while (shell_buffer[arg_idx] == ' ') arg_idx++;

        if (shell_buffer[arg_idx] == '\0') {
            terminal_print("Uso: mkdir [CAMINHO_DA_PASTA]\n");
        } else {
            // Permite criar pastas em ramificações profundas
            fs_create_directory(&shell_buffer[arg_idx]);
        }
    }
    else if (str_prefix_compare(shell_buffer, "cd")) {
        int arg_idx = 2;
        while (shell_buffer[arg_idx] == ' ') arg_idx++;

        if (shell_buffer[arg_idx] == '\0') {
            fs_change_directory("");
        } else {
            // Envia o path multinível de navegação diretamente
            fs_change_directory(&shell_buffer[arg_idx]);
        }
    }
    else {
        terminal_print("Comando nao encontrado: ");
        terminal_print(shell_buffer);
    }

    buffer_idx = 0;
    shell_print_prompt();
}

void shell_input_char(char c) {
    if (c == '\n') {
        shell_execute_command();
        return;
    }
    if (c == '\b') {
        if (buffer_idx > 0) {
            buffer_idx--;
            terminal_putchar(c);
        }
        return;
    }
    if (buffer_idx < BUFFER_SIZE - 1) {
        shell_buffer[buffer_idx++] = c;
        terminal_putchar(c);
    }
}