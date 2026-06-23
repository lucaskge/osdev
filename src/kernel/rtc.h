#ifndef RTC_H
#define RTC_H

// Estrutura que armazena os dados de tempo legíveis
struct rtc_time {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned int year;
};

// Lê o chip CMOS e atualiza a estrutura com o tempo atual do PC
void rtc_get_time(struct rtc_time* time);

#endif