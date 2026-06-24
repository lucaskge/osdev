#include "vga.h"

// Lista de strings correspondente às 32 exceções da arquitetura x86 Intel
const char* exception_messages[] = {
    "Division By Zero (Divisao por Zero)",
    "Debug Exception",
    "Non Maskable Interrupt",
    "Breakpoint Exception",
    "Into Detected Overflow",
    "Out of Bounds Exception",
    "Invalid Opcode (Instrucao Invalida/Ponteiro Corrompido)",
    "No Coprocessor Exception",
    "Double Fault (Falha Dupla Catastrofica)",
    "Coprocessor Segment Overrun",
    "Bad TSS Exception",
    "Segment Not Present",
    "Stack Fault Exception (Estouro de Pilha)",
    "General Protection Fault (Acesso de Memoria Proibido)",
    "Page Fault (Falta de Pagina/Endereco nao mapeado pela CPU)",
    "Unknown Interrupt Exception",
    "Coprocessor Fault",
    "Alignment Check Exception",
    "Machine Check Exception",
    "Reserved Excepetion 19",
    "Reserved Excepetion 20",
    "Reserved Excepetion 21",
    "Reserved Excepetion 22",
    "Reserved Excepetion 23",
    "Reserved Excepetion 24",
    "Reserved Excepetion 25",
    "Reserved Excepetion 26",
    "Reserved Excepetion 27",
    "Reserved Excepetion 28",
    "Reserved Excepetion 29",
    "Reserved Excepetion 30",
    "Reserved Excepetion 31"
};

// Estrutura espelhada da Pilha que o nosso código Assembly empurrou
// struct cpu_state {
//     unsigned int ds;
//     unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Empurrados pelo pusha
//     unsigned int int_no, err_code;                       // Empurrados manualmente no stub
//     unsigned int eip, cs, eflags, useresp, ss;           // Empurrados automaticamente pela CPU
// };

// Estrutura espelhada da Pilha baseada estritamente no seu boot.asm
struct cpu_state {
    unsigned int gs, fs, es, ds;                         // Empurrados por último no seu código
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Empurrados pelo pusha
    unsigned int int_no, err_code;                       // Empurrados manualmente/hardware no stub
    unsigned int eip, cs, eflags, useresp, ss;           // Empurrados automaticamente pela CPU x86
};

// O Kernel Panic centralizado. Se a CPU disparar uma exceção, ela cai aqui!
void exception_handler(struct cpu_state regs) {
    terminal_clear();
    
    // Altera as cores do VGA de forma direta se o seu vga.h der suporte, 
    // ou apenas imprime o alerta de erro crítico chamativo.
    terminal_print("===================================================================\n");
    terminal_print("                  !!! KERNEL PANIC DETECTED !!!                    \n");
    terminal_print("===================================================================\n\n");
    terminal_print("O MeuOS encontrou um erro critico de hardware e foi interrompido.\n");
    terminal_print("Excecao disparada pela CPU: ");
    
    if (regs.int_no < 32) {
        terminal_print(exception_messages[regs.int_no]);
    } else {
        terminal_print("Unknown Exception");
    }
    terminal_print("\n\n");

    terminal_print("--- DUMP DO ESTADO DOS REGISTRADORES DOS VALORES DA CPU ---\n");
    
    // Imprime o endereço exato do código que causou o travamento (EIP)
    terminal_print("  EIP (Ponteiro de Instrucao): 0x");
    char num_buf[16];
    unsigned int temp = regs.eip;
    int i = 0;
    while (temp > 0) {
        int rem = temp % 16;
        if (rem < 10) num_buf[i++] = '0' + rem;
        else num_buf[i++] = 'A' + (rem - 10);
        temp /= 16;
    }
    if (i == 0) terminal_print("0");
    else {
        for (int j = i - 1; j >= 0; j--) terminal_putchar(num_buf[j]);
    }
    
    terminal_print("\n  CS: ");
    // Você pode expandir esse dump para cuspir EAX, EBX se desejar investigar a fundo bugs complexos

    terminal_print("\n\nO sistema foi congelado por seguranca para evitar corrupcao do HD.\n");
    terminal_print("Por favor, reinicie a maquina virtual.");

    // Trava a CPU em loop eterno seguro sem gastar energia (hlt)
    while(1) {
        __asm__ volatile("hlt");
    }
}
