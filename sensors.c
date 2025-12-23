#include <msp430f5529.h>

#define GAS_THRESHOLD  1800   // Adjust experimentally

// External function declarations (from other .c files)
extern void lcd_cmd(unsigned char cmd);
extern void lcd_print(const char *s, unsigned char crs);
extern void lcd_show_gas(unsigned int value);
extern void servo_open(void);

// Global flags
volatile unsigned char ir_intrusion_flag = 0;
volatile unsigned char gas_alert_flag = 0;
volatile unsigned char sos_button_flag = 0;
volatile unsigned char gas_check_flag = 0;
volatile unsigned char flame_detect_flag = 0;
volatile unsigned char fire_override_active = 0;

void init_gpio(void)
{
    /* BUZZER on P1.6 */
    P1DIR |= BIT6;
    P1SEL &= ~BIT6;
    P1OUT &= ~BIT6;

    /* IR SENSOR on P1.3 */
    P1DIR &= ~BIT3;
    P1REN |= BIT3;
    P1OUT |= BIT3;
    P1IES |= BIT3;
    P1IFG &= ~BIT3;
    P1IE  |= BIT3;

    /* S1 PUSH BUTTON on P2.1 */
    P2DIR &= ~BIT1;
    P2REN |= BIT1;
    P2OUT |= BIT1;
    P2IES |= BIT1;
    P2IFG &= ~BIT1;
    P2IE  |= BIT1;

    /* FLAME SENSOR on P2.7 */
    P2DIR &= ~BIT7;
    P2REN |= BIT7;
    P2OUT |= BIT7;
    P2IES |= BIT7;
    P2IFG &= ~BIT7;
    P2IE  |= BIT7;
}

void init_timer(void)
{
    TA0CTL = TASSEL_1 | MC_1 | TACLR;   // ACLK, Up mode
    TA0CCR0 = 16384;                    // ~0.5 sec @ 32kHz
    TA0CCTL0 = CCIE;                    // Enable interrupt
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A_ISR(void)
{
    gas_check_flag = 1;
    __bic_SR_register_on_exit(LPM0_bits);
}

void init_adc_mq6(void)
{
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;   // Sample & hold, ADC ON
    ADC12CTL1 = ADC12SHP;                // Use sampling timer
    ADC12MCTL0 = ADC12INCH_0;            // Channel A0 (P6.0)
    ADC12CTL0 |= ADC12ENC;               // Enable conversion
}

unsigned int read_mq6_adc(void)
{
    ADC12CTL0 |= ADC12SC;                 // Start conversion
    while (ADC12CTL1 & ADC12BUSY);        // Wait
    return ADC12MEM0;
}

void check_gas_level(void)
{
    static unsigned char lcd_cnt = 0;
    unsigned int gas_value;

    gas_value = read_mq6_adc();

    if(++lcd_cnt >= 4)   // Update LCD every ~2 sec
    {
        lcd_show_gas(gas_value);
        lcd_cnt = 0;
    }

    if(gas_value > GAS_THRESHOLD)
        gas_alert_flag = 1;
}

void handle_gas_alert(void)
{
    buzzer_beep();
    __delay_cycles(3000000);
}

void handle_flame_detect(void)
{
    fire_override_active = 1;
    lcd_cmd(0x01);
    lcd_print("FIRE ALERT", 0x80);
    servo_open();
    buzzer_beep();
    __delay_cycles(500000);
}

void handle_ir_intrusion(void)
{
    buzzer_beep();
}

void buzzer_beep(void)
{
    P1OUT |= BIT6;
    __delay_cycles(500000);
    P1OUT &= ~BIT6;
}

void buzzer_1sec(void)
{
    P1OUT |= BIT6;
    __delay_cycles(1000000);
    P1OUT &= ~BIT6;
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1_ISR(void)
{
    if(P1IFG & BIT3)   // IR sensor
    {
        ir_intrusion_flag = 1;
        P1IFG &= ~BIT3;
    }
    __bic_SR_register_on_exit(LPM0_bits);
}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2_ISR(void)
{
    if(P2IFG & BIT1)   // S1 button
    {
        sos_button_flag = 1;
        P2IFG &= ~BIT1;
    }

    if(P2IFG & BIT7)   // Flame sensor
    {
        flame_detect_flag = 1;
        P2IFG &= ~BIT7;
    }
    __bic_SR_register_on_exit(LPM0_bits);
}
