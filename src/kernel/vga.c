#include "vga.h"

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define DEFAULT_COLOR 0x0F

int term_column = 0;
int term_row = 0;
volatile char* video_mem = (volatile char*)VGA_ADDRESS;

void terminal_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = (y * VGA_WIDTH + x) * 2;
            video_mem[index] = ' ';
            video_mem[index + 1] = DEFAULT_COLOR;
        }
    }
    term_column = 0;
    term_row = 0;
}

void terminal_putchar(char c) {
    // TRATAMENTO DO BACKSPACE '\b'
    if (c == '\b') {
        if (term_column > 2) { // Garante que não vai apagar o prompt "> " do Shell
            term_column--; // Volta uma coluna para trás
            const int index = (term_row * VGA_WIDTH + term_column) * 2;
            video_mem[index] = ' '; // Substitui o caractere antigo por um espaço vazio
            video_mem[index + 1] = DEFAULT_COLOR;
        }
        return;
    }

    // Trata o caractere de nova linha '\n'
    if (c == '\n') {
        term_column = 0;
        term_row++;
        return;
    }

    // Renderiza o caractere padrão na tela
    const int index = (term_row * VGA_WIDTH + term_column) * 2;
    video_mem[index] = c;
    video_mem[index + 1] = DEFAULT_COLOR;
    
    term_column++;
    if (term_column >= VGA_WIDTH) {
        term_column = 0;
        term_row++;
    }
}

void terminal_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}