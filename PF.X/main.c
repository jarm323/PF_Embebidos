#include <xc.h>
#include <pic18f4550.h>
#include "LCD.h"
#include <stdio.h>

#pragma config PLLDIV = 5
#pragma config CPUDIV = OSC1_PLL2
#pragma config FOSC = HSPLL_HS
#pragma config PWRT = ON
#pragma config WDT = OFF
#pragma config MCLRE = ON
#pragma config PBADEN = OFF
#pragma config LVP = OFF
#pragma config CCP2MX = ON

#define _XTAL_FREQ 48000000UL

#define RELAY     LATDbits.LATD0
#define RELAY_TRIS TRISDbits.TRISD0
#define PWM_LED   LATCbits.LATC1
#define PWM_TRIS  TRISCbits.TRISC1

// Variables para override
char override = 0;
char manual_state = 0;

void ADC_Init() {
    TRISAbits.TRISA1 = 1;
    ADCON1 = 0x0E;
    ADCON2 = 0xBE;
    ADCON0 = 0b00000101;
}

unsigned int Read_Temperature() {
    __delay_us(20);
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);
    return (ADRESH << 8) | ADRESL;
}

void PWM_Init() {
    TRISCbits.TRISC1 = 0;
    TRISCbits.TRISC2 = 0;
    T2CONbits.TMR2ON = 1;
    PR2 = 124;
    CCP1CON = 0x0C;
    T2CON = 0b00000110;
    CCPR1L = 0;
}

// ---------- UART ----------------
void UART_Init() {
    TRISCbits.RC6 = 0; // TX (salida)
    TRISCbits.RC7 = 1; // RX (entrada)

    SPBRG = 77;              // 9600 baudios @ 24MHz, BRGH = 0
    TXSTAbits.BRGH = 0;      // Low speed
    BAUDCONbits.BRG16 = 0;   // 8-bit baud generator

    TXSTAbits.SYNC = 0;      // Modo asíncrono
    RCSTAbits.SPEN = 1;      // Habilita el puerto serial (TX y RX)

    TXSTAbits.TXEN = 1;      // Habilita la transmisión
    RCSTAbits.CREN = 1;      // Recepción continua
}


void UART_Write(char data) {
    while (!TXSTAbits.TRMT);
    TXREG = data;
}

void UART_Write_Text(const char *text) {
    while (*text) UART_Write(*text++);
}
// -------------------------------

void main(void) {
    TRISD = 0x00;
    TRISE = 0x00;
    
    LCD_CONFIG();
    __delay_ms(100);
    
    BORRAR_LCD();
    __delay_ms(5);
    POS_CURSOR(1, 1);
    ESCRIBE_MENSAJE("Temperatura:", 12);
    POS_CURSOR(2, 1);
    ESCRIBE_MENSAJE("VENT:OFF", 8);
    
    ADC_Init();
    PWM_Init();
    UART_Init();
    RELAY_TRIS = 0;

    unsigned int counter_uart = 0;

    while (1) {
        unsigned int adc = Read_Temperature();
        unsigned int temp = (adc * 500) / 1024;

        // Leer comandos UART
        if (PIR1bits.RCIF) {
            char cmd = RCREG;
            if (cmd == '1') {
                override = 1;
                manual_state = 1;
                UART_Write_Text("Override ON\r\n");
            } else if (cmd == '0') {
                override = 1;
                manual_state = 0;
                UART_Write_Text("Override OFF\r\n");
            } else if (cmd == 'a') {
                override = 0;
                UART_Write_Text("Automatico\r\n");
            }
        }

        
        if (override)
            RELAY = manual_state;
        else
            RELAY = (temp > 30) ? 1 : 0;

        // PWM (brillo LED)
        int t = (temp < 20) ? 0 : (temp - 20);
        if (t > 30) t = 30;
        unsigned char pwm = (t * t * 255) / (30 * 30);

        CCPR1L = pwm >> 2;
        CCP1CONbits.DC1B = pwm & 0x03;
        __delay_ms(100);

        // Mostrar temperatura en LCD
        POS_CURSOR(1, 13);
        char temp_str[5];
        sprintf(temp_str, "%3dC", temp);
        ESCRIBE_MENSAJE(temp_str, 4);

        // Estado del rele
        POS_CURSOR(2, 6);
        ESCRIBE_MENSAJE(RELAY ? "ON " : "OFF", 3);

        // Enviar temperatura por UART
        char uart_buf[20];
        sprintf(uart_buf, "Temp=%dC\r\n", temp);
        UART_Write_Text(uart_buf);


        __delay_ms(500);
    }
}
    