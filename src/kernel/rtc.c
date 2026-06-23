#include "rtc.h"

// Importamos a comunicação com as portas físicas do kernel.c
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

// Função para verificar se o RTC está ocupado atualizando os dados (bit 7 do registrador A)
static int rtc_is_updating(void) {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

// Lê um registrador específico da CMOS
static unsigned char rtc_read_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// Converte um valor codificado em BCD para decimal puro (C comum)
static unsigned char bcd_to_bin(unsigned char bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

void rtc_get_time(struct rtc_time* time) {
    // Aguarda o chip terminar qualquer atualização interna para não ler dados corrompidos
    while (rtc_is_updating());

    unsigned char status_b;

    // 1. Lê os valores brutos do chip
    time->second = rtc_read_register(0x00);
    time->minute = rtc_read_register(0x02);
    time->hour   = rtc_read_register(0x04);
    time->day    = rtc_read_register(0x07);
    time->month  = rtc_read_register(0x08);
    time->year   = rtc_read_register(0x09);

    status_b = rtc_read_register(0x0B);

    // 2. Se o status indicar que os dados estão em formato BCD (padrão da maioria dos PCs), converte
    if (!(status_b & 0x04)) {
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour   = bcd_to_bin(time->hour);
        time->day    = bcd_to_bin(time->day);
        time->month  = bcd_to_bin(time->month);
        time->year   = bcd_to_bin(time->year);
    }

    // 3. O chip armazena o ano com apenas 2 dígitos (ex: 26 para 2026). Ajustamos para 4 dígitos.
    time->year += 2000;
}