section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                       ; Número mágico do Multiboot2
    dd 0                                ; Arquitetura 0 (Modo protegido 32-bits i386)
    dd header_end - header_start        ; Tamanho do cabeçalho
    dd -(0xe85250d6 + 0 + (header_end - header_start)) ; Checksum de validação

    align 8
    dw 0                                ; Tag de finalização: tipo
    dw 0                                ; Tag de finalização: flags
    dd 8                                ; Tag de finalização: tamanho
header_end:

section .text
bits 32
global start
extern kernel_main
extern exception0_handler
extern keyboard_handler
global isr0
global isr33
global isr_ignore                       ; Torna o tratador padrão visível para o C

; --- TRATADORES DE INTERRUPÇÃO (ISRs) ---

; ISR 0: Tratador de Divisão por Zero
align 4
isr0:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    call exception0_handler
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

; ISR 33: Tratador do Teclado (IRQ1)
align 4
isr33:
    pusha                               ; Salva os registradores gerais (EAX, ECX, etc)
    push ds                             ; Salva registradores de segmento de dados
    push es
    push fs
    push gs

    mov ax, 0x10                        ; Força o segmento de dados do Kernel (0x10)
    mov ds, ax
    mov es, ax

    call keyboard_handler               ; Chama a lógica em C para processar a tecla

    mov al, 0x20                        ; Envia o sinal de Fim de Interrupção (EOI)
    out 0x20, al                        ; Sinaliza o PIC Master na porta 0x20

    pop gs                              ; Restaura os segmentos na ordem inversa
    pop fs
    pop es
    pop ds
    popa                                ; Restaura os registradores gerais
    iret                                ; Retorna ao código principal com segurança

; ISR Padrão: Captura e descarta interrupções espúrias/indesejadas
align 4
isr_ignore:
    pusha
    mov al, 0x20                        ; Avisa o PIC que recebemos o sinal
    out 0x20, al                        ; Evita travamento do chip PIC Master
    out 0xA0, al                        ; Evita travamento do chip PIC Slave
    popa
    iret                                ; Ignora e retorna sem quebrar a CPU

; --- PONTO DE ENTRADA DO SISTEMA OPERACIONAL ---
start:
    cli                                 ; Garante interrupções desligadas no arranque
    mov esp, stack_top                  ; Inicializa o ponteiro da pilha (Stack Pointer)

    lgdt [gdt_descriptor]               ; Carrega nossa GDT estável
    jmp 0x08:.init_segments             ; Salto longo para atualizar CS para 0x08

.init_segments:
    mov ax, 0x10                        ; Sincroniza todos os registradores de dados
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call kernel_main                    ; Executa a configuração da IDT e PIC no C

    extern idtp
    lidt [idtp]                         ; Carrega a IDT estruturada na CPU

    sti                                 ; Ativa as interrupções com segurança total

.loop:
    hlt                                 ; Coloca a CPU em repouso aguardando o teclado
    jmp .loop                           ; Se acordar, volta a dormir

; --- DEFINIÇÃO DA GDT ESTÁVEL ---
align 4
gdt_start:
    ; Entrada 0: Segmento nulo obrigatório
    dd 0x0
    dd 0x0

    ; Entrada 1: Segmento de Código do Kernel (Seletor 0x08)
    ; Base=0, Limite=0xFFFFF, Granularidade=4KB, 32-bits, Código Executável/Leitura
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00

    ; Entrada 2: Segmento de Dados do Kernel (Seletor 0x10)
    ; Base=0, Limite=0xFFFFF, Granularidade=4KB, 32-bits, Dados Leitura/Escrita
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1          ; Tamanho da GDT
    dd gdt_start                        ; Endereço base da GDT

section .bss
align 16
stack_bottom:
    resb 16384                          ; Aloca 16KB de pilha
stack_top: