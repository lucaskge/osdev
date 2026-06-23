#include "shell.h"
#include "vga.h"
#include "fs.h"
#include "rtc.h"
#include "task.h"

#define BUFFER_SIZE 256

char shell_buffer[BUFFER_SIZE];
int buffer_idx = 0;

extern void outb(unsigned short port, unsigned char val);
extern void start_multitasking(void);

int str_compare(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return (s1[i] == '\0' && s2[i] == '\0');
}

void shell_print_prompt(void) {
    terminal_print("\n> ");
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

void shell_execute_command(void) {
    if (buffer_idx == 0) {
        shell_print_prompt();
        return;
    }

    shell_buffer[buffer_idx] = '\0';
    terminal_print("\n");

    if (str_compare(shell_buffer, "help")) {
        terminal_print("Comandos disponiveis:\n");
        terminal_print("  help     - Mostra esta lista de comandos\n");
        terminal_print("  clear    - Limpa a tela do terminal\n");
        terminal_print("  about    - Exibe informacoes do sistema\n");
        terminal_print("  ls       - Lista os arquivos do diretorio raiz\n");
        terminal_print("  cat      - Abre o arquivo README.TXT\n");
        terminal_print("  time     - Exibe a data e hora atual do chip RTC\n");
        terminal_print("  task     - Inicia o agendador de multitarefa paralela\n");
        terminal_print("  shutdown - Desliga a maquina virtual");
    } 
    else if (str_compare(shell_buffer, "clear")) {
        terminal_clear();
    } 
    else if (str_compare(shell_buffer, "about")) {
        terminal_print("MeuOS v1.0.0 Bare Metal x86_32\n");
        terminal_print("Suporte a multi-threads nativo.");
    } 
    else if (str_compare(shell_buffer, "ls")) {
        fs_list_directory();
    }
    else if (str_compare(shell_buffer, "cat")) {
        fs_read_file("README  TXT");
    }
    else if (str_compare(shell_buffer, "time")) {
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
    else if (str_compare(shell_buffer, "task")) {
        terminal_print("Passando o controle para os lacos paralelos nativos...\n");
        task_init();           // Monta as duas pilhas estáveis na RAM
        start_multitasking();  // Dispara o carrossel isolando o Shell
    }
    else if (str_compare(shell_buffer, "shutdown")) {
        sys_shutdown();
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