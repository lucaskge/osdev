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
global isr_ignore

; Exportação da rotina de troca de contexto cooperativa que funcionou
global switch_context_asm
extern current_task_esp_ptr
extern next_task_esp_val

; --- TRATADORES DE INTERRUPÇÃO (ISRs) ---

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

align 4
isr33:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    call keyboard_handler
    mov al, 0x20
    out 0x20, al
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

align 4
isr_ignore:
    pusha
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    popa
    iret


; --- TROCA DE CONTEXTO COOPERATIVA PURA ---
align 4
switch_context_asm:
    pushf                       ; Salva as FLAGS da tarefa antiga
    push cs                     ; Salva o CS antigo
    pusha                       ; Salva os 8 registradores gerais antigos

    ; Salva o ESP físico atual no ponteiro que o C configurou
    mov eax, [current_task_esp_ptr]
    mov [eax], esp

    ; Carrega o novo ESP físico que o C escolheu
    mov esp, [next_task_esp_val]

    popa                        ; Restaura os registradores gerais da nova tarefa
    pop eax                     ; Descarta o CS de controle
    pop eax                     ; Descarta as FLAGS de controle
    ret                         ; Salta para o endereço de execução (EIP) da nova tarefa


; --- PONTO DE ENTRADA DO SISTEMA OPERACIONAL ---
start:
    cli
    mov esp, stack_top

    lgdt [gdt_descriptor]
    jmp 0x08:.init_segments

.init_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call kernel_main

    extern idtp
    lidt [idtp]

    sti                         ; Ativa apenas as interrupções de teclado seguras

.loop:
    hlt
    jmp .loop

align 4
gdt_start:
    dd 0x0
    dd 0x0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: