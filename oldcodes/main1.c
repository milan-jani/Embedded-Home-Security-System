#include <msp430f5529.h>
#include <string.h>   // 🔧 COPILOT ADDED: Required for strcmp() and strlen()



volatile unsigned char ir_intrusion_flag = 0;
volatile unsigned char gas_alert_flag = 0;
volatile unsigned char sos_button_flag = 0;
volatile unsigned char gas_check_flag = 0;
volatile unsigned char flame_detect_flag = 0;
volatile unsigned char uart_rx_flag = 0;
volatile char uart_rx_char;
volatile unsigned char fire_override_active = 0;
volatile unsigned char pass_mode_active = 0;

char pass_buf[5];
unsigned char pass_index = 0;   // Password buffer index (0-3 for 4 digits)



void init_gpio(void);
void handle_ir_intrusion(void);
void buzzer_beep(void);
void init_adc_mq6(void);
unsigned int read_mq6_adc(void);
void check_gas_level(void);
void handle_gas_alert(void);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_print(const char *s, unsigned char crs);


#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01
#define PASSWORD "1234"
#define PASS_LEN 4


// void init_clock(void)
// {
//     CSCTL0_H = CSKEY >> 8;        // Unlock CS registers
//     CSCTL1 = DCOFSEL_0;           // DCO = 1 MHz
//     CSCTL2 = SELS__DCOCLK;        // SMCLK = DCO
//     CSCTL3 = DIVS__1;             // SMCLK /1
//     CSCTL0_H = 0;                 // Lock CS registers
// }


void init_uart(void)
{
    P4SEL |= BIT4 | BIT5;     // TX, RX

    UCA1CTL1 |= UCSWRST;
    // 🔧 COPILOT FIXED: Changed from ACLK to SMCLK for reliable 9600 baud
    UCA1CTL1 = UCSSEL__SMCLK | UCSWRST;   // SMCLK = 1 MHz (default DCO)

    // 🔧 COPILOT FIXED: Baud rate settings for 9600 @ 1MHz SMCLK
    UCA1BR0 = 104;      // 1MHz / 9600 = 104.166
    UCA1BR1 = 0;
    UCA1MCTL = UCBRS_1; // Fractional part

    UCA1CTL1 &= ~UCSWRST;
    UCA1IE  |= UCRXIE;
}

#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    // 🔧 COPILOT FIXED: Removed lcd_cmd() call - NEVER do I2C/LCD in UART ISR!
    // ISR must be fast: read char, set flag, exit
    if(UCA1IFG & UCRXIFG)
    {
        uart_rx_char = UCA1RXBUF;
        uart_rx_flag = 1;
        __bic_SR_register_on_exit(LPM0_bits);
    }
}

void lcd_show_pass_char(char c)
{
    // Show password digit at position 6+ on line 2 (after "PASS:")
    // 0xC0 = line 2 start, +5 for "PASS:", then +pass_index
    lcd_cmd(0xC0 + 5 + pass_index);   // Line-2 position after "PASS:"
    lcd_data(c);
   
}



void handle_password_input(void)
{
    /* Enter password mode as soon as a digit arrives */
    pass_mode_active = 1;

    if(uart_rx_char >= '0' && uart_rx_char <= '9')
    {
        // Store digit in buffer at current index (0-3)
        pass_buf[pass_index] = uart_rx_char;
        lcd_show_pass_char(uart_rx_char);   // show digit on LCD after "PASS:"
        pass_index++;

        if(pass_index >= PASS_LEN)   // All 4 digits received
        {
            pass_buf[PASS_LEN] = '\0';

            lcd_cmd(0x01);   // clear LCD

            if(strcmp(pass_buf, PASSWORD) == 0)
            {
                lcd_print("DOOR UNLOCKED", 0x80);
                servo_open();    // 90°
            }
            else
            {
                lcd_print("WRONG PASS",0x80);
                servo_close();   // 0°
            }

            /* Exit password mode */
            pass_index = 0;
            pass_mode_active = 0;
        }
    }
}



// Timer A1 PWM initialization for servo on P2.4
void init_servo_pwm(void)
{
    // Configure P2.4 as TA1.1 output (servo control pin)
    P2DIR |= BIT4;
    P2SEL |= BIT4;
    
    // Timer A1 setup for 50Hz PWM (20ms period)
    TA1CCR0 = 20000;              // 20ms period @ 1MHz SMCLK
    TA1CCTL1 = OUTMOD_7;          // Reset/Set mode
    TA1CCR1 = 1500;               // Default position (90 degrees)
    TA1CTL = TASSEL_2 | MC_1;     // SMCLK, Up mode
}

void servo_open(void)
{
    TA1CCR1 = 1500;   // ~1.5 ms → 90°
}

void servo_close(void)
{
    TA1CCR1 = 1000;   // ~1 ms → 0°
}

void handle_flame_detect(void)
{
    fire_override_active = 1;
    lcd_cmd(0x01);
    lcd_print("FIRE ALERT",0x80);
    servo_open();
    buzzer_beep();
    __delay_cycles(500000);
}

// void handle_flame_detect(void)
// {
//     // Audible alert
//     buzzer_beep();

//     // LCD message
//     lcd_cmd(0x01);            // Clear
//     lcd_print("FIRE ALERT",0x80);
//     lcd_cmd(0x01);            // Clear

//     __delay_cycles(500000);
// }


void lcd_cmd(unsigned char cmd)
{
    unsigned char data;

    data = (cmd & 0xF0) | LCD_BACKLIGHT;
    i2c_write(data | LCD_ENABLE);
    i2c_write(data);

    data = ((cmd << 4) & 0xF0) | LCD_BACKLIGHT;
    i2c_write(data | LCD_ENABLE);
    i2c_write(data);

    __delay_cycles(2000);
}

void lcd_data(unsigned char data)
{
    unsigned char d;

    d = (data & 0xF0) | LCD_BACKLIGHT | LCD_RS;
    i2c_write(d | LCD_ENABLE);
    i2c_write(d);

    d = ((data << 4) & 0xF0) | LCD_BACKLIGHT | LCD_RS;
    i2c_write(d | LCD_ENABLE);
    i2c_write(d);

    __delay_cycles(2000);
}

void lcd_init(void)
{
    __delay_cycles(50000);   // LCD power-up wait

    lcd_cmd(0x33);
    lcd_cmd(0x32);
    lcd_cmd(0x28);   // 4-bit, 2 line
    lcd_cmd(0x0C);   // Display ON, cursor OFF
    lcd_cmd(0x06);   // Entry mode
    lcd_cmd(0x01);   // Clear
}
    void lcd_print(const char *s, unsigned char crs){
        lcd_cmd(crs);   // for Line choosing 
        int i=0;
        char f=0;
        for (i=0; i<strlen(s); i++) {
            lcd_data(s[i]);
            if(i>15 && !f){
            lcd_cmd(0xC0);
            f=1;
            } 
        }
        __delay_cycles(2000000);
        
    }

void init_gpio(void)
{
    /* BUZZER */
    P1DIR |= BIT6;
    P1SEL &= ~BIT6;
    P1OUT &= ~BIT6;

    /* IR SENSOR */
    P1DIR &= ~BIT3;
    P1REN |= BIT3;
    P1OUT |= BIT3;

    P1IES |= BIT3;
    P1IFG &= ~BIT3;
    P1IE  |= BIT3;

    /* ---------- S1 PUSH BUTTON (P2.1) ---------- */
P2DIR &= ~BIT1;     // Input
P2REN |= BIT1;      // Enable pull resistor
P2OUT |= BIT1;      // Pull-up
P2IES |= BIT1;      // High → Low (button press)
P2IFG &= ~BIT1;     // Clear flag
P2IE  |= BIT1;      // Enable interrupt

    /* ---------- FLAME SENSOR (P2.7) ---------- */
P2DIR &= ~BIT7;     // Input
P2REN |= BIT7;      // Enable pull resistor
P2OUT |= BIT7;      // Pull-up

P2IES |= BIT7;      // High → Low interrupt (flame detect)
P2IFG &= ~BIT7;     // Clear flag
P2IE  |= BIT7;      // Enable interrupt


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
    gas_check_flag = 1;   // Time to check MQ sensor
    __bic_SR_register_on_exit(LPM0_bits);
}

void init_i2c(void)
{
    P3SEL |= BIT0 | BIT1;          // SDA, SCL

    UCB0CTL1 |= UCSWRST;
    UCB0CTL0 = UCMST | UCMODE_3 | UCSYNC; // I2C master
    UCB0CTL1 = UCSSEL_2 | UCSWRST;        // SMCLK
    UCB0BR0 = 10;                          // ~100kHz
    UCB0BR1 = 0;
    UCB0I2CSA = 0x27;                     // LCD I2C address (common)
    UCB0CTL1 &= ~UCSWRST;
}

void i2c_write(unsigned char data)
{
    while(UCB0CTL1 & UCTXSTP);
    UCB0CTL1 |= UCTR | UCTXSTT;
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = data;
    while(!(UCB0IFG & UCTXIFG));
    UCB0CTL1 |= UCTXSTP;
}

void lcd_show_gas(unsigned int value)
{   
    lcd_cmd(0x80);   // Line 1

    lcd_data('G');
    lcd_data('A');
    lcd_data('S');
    lcd_data(':');
    lcd_data(' ');
   
    char buf[5];
    int i = 0;

    if(value == 0)
        buf[i++] = '0';
    else
    {
        while(value > 0)
        {
            buf[i++] = (value % 10) + '0';
            value /= 10;
        }
    }

    while(i--)
        lcd_data(buf[i]);
}



void buzzer_beep(void)
{
    P1OUT |= BIT6;
    __delay_cycles(500000);
    P1OUT &= ~BIT6;
}

void handle_ir_intrusion(void)
{
  //  uart_print_uint(1);   
    buzzer_beep();
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1_ISR(void)
{
    if(P1IFG & BIT3)
    {
        ir_intrusion_flag = 1;
        P1IFG &= ~BIT3;
    }

    __bic_SR_register_on_exit(LPM0_bits);
}
#pragma vector=PORT2_VECTOR
__interrupt void Port_2_ISR(void)
{
    if(P2IFG & BIT1)   // S1 pressed
    {
        sos_button_flag = 1;
        P2IFG &= ~BIT1;
    }

    if(P2IFG & BIT7)   // 🔥 Flame detected
    {
        flame_detect_flag = 1;
        P2IFG &= ~BIT7;
    }

    __bic_SR_register_on_exit(LPM0_bits);
}

void buzzer_1sec(void)
{
    P1OUT |= BIT6;          // Buzzer ON
    __delay_cycles(1000000); // ~1 sec @ 1 MHz
    P1OUT &= ~BIT6;         // Buzzer OFF
}


void init_adc_mq6(void)
{
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;   // Sample & hold, ADC ON
    ADC12CTL1 = ADC12SHP;                // Use sampling timer
    ADC12MCTL0 = ADC12INCH_0;             // Channel A0 (P6.0)
    ADC12CTL0 |= ADC12ENC;                // Enable conversion
}

unsigned int read_mq6_adc(void)
{
    ADC12CTL0 |= ADC12SC;                 // Start conversion
    while (ADC12CTL1 & ADC12BUSY);        // Wait
    return ADC12MEM0;                     // Return ADC value
}

#define GAS_THRESHOLD  1800   // Adjust experimentally
void check_gas_level(void)
{
    static unsigned char lcd_cnt = 0;
    unsigned int gas_value;

    gas_value = read_mq6_adc();
    // lcd_cmd(0x01);
    if(++lcd_cnt >= 4)   // ~2 sec
    {
        lcd_show_gas(gas_value);
        lcd_cnt = 0;
    }

    if(gas_value > GAS_THRESHOLD)
        gas_alert_flag = 1;
}


void handle_gas_alert(void)
{
    // Continuous alarm
   buzzer_beep();
   __delay_cycles(3000000);   // 🔧 COPILOT FIXED: Was _delay_cycles (single underscore)
}





int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;
   // init_clock(); 
    init_gpio();        // IR, S1, buzzer
    init_uart();
    init_adc_mq6();     // MQ ADC
    init_timer();       // Gas check timer
    init_i2c();         // LCD / I2C  ⭐ REQUIRED
    lcd_init();
    init_servo_pwm();   // 🔧 COPILOT ADDED: Initialize Timer A1 for servo PWM
    __bis_SR_register(GIE);
    lcd_cmd(0x01); // clear the lcd;
    lcd_print("SECURITY SYSTEM", 0x80);
     
    lcd_print("By Milan Jani",0xC0);
    lcd_cmd(0x01); // clear the lcd;
    
    servo_open();
    __delay_cycles(1000000);   // 🔧 COPILOT FIXED: Was _delay_cycles
    servo_close();
    
    

    
     __delay_cycles(500000);
    while(1)
    {
        if(sos_button_flag)
        {
            buzzer_1sec();
            sos_button_flag = 0;
        }
        if(flame_detect_flag)   // 🔥 HIGH PRIORITY
        {
            handle_flame_detect();
            flame_detect_flag = 0;
        }
        if(!fire_override_active)
        {
            if(gas_check_flag && !pass_mode_active)
            {
                check_gas_level();        // LCD line-1 GAS
                lcd_cmd(0xC0);
                lcd_print("PASS:",0xC0);
                gas_check_flag = 0;
            }

            if(uart_rx_flag)
            {
                buzzer_beep();
                handle_password_input();
                uart_rx_flag = 0;
            }

            if(gas_alert_flag)
            {
                handle_gas_alert();
                gas_alert_flag = 0;
            }

            if(ir_intrusion_flag)
            {
                handle_ir_intrusion();
                ir_intrusion_flag = 0;
            }
        }
        __bis_SR_register(LPM0_bits);
        
    }
}














// #include <msp430f5529.h>

// volatile unsigned char ir_intrusion_flag = 0;

// int main(void)
// {
//     WDTCTL = WDTPW | WDTHOLD;

//     /* ---------- OUTPUT : BUZZER ---------- */
//     P1DIR |= BIT6;
//     P1SEL &= ~BIT6;      // GPIO mode
//     P1OUT &= ~BIT6;

//     /* ---------- INPUT : IR SENSOR ---------- */
//     P1DIR &= ~BIT3;      // Input
//     P1REN |= BIT3;       // Pull resistor
//     P1OUT |= BIT3;       // Pull-up

//     P1IES |= BIT3;       // High → Low interrupt (IR detect)
//     P1IFG &= ~BIT3;      // Clear flag
//     P1IE  |= BIT3;       // Enable interrupt

//     __bis_SR_register(GIE);   // Global interrupt enable

//     /* ---------- MAIN LOOP ---------- */
//     while(1)
//     {
//         if(ir_intrusion_flag)
//         {
//             P1OUT |= BIT6;    // Buzzer ON
//             __delay_cycles(800000);
//             P1OUT &= ~BIT6;   // Buzzer OFF

//             ir_intrusion_flag = 0;
//         }

//         __bis_SR_register(LPM0_bits);  // Sleep till event
//     }
// }

// #pragma vector=PORT1_VECTOR
// __interrupt void Port_1_ISR(void)
// {
//     if(P1IFG & BIT3)   // IR interrupt
//     {
//         ir_intrusion_flag = 1;
//         P1IFG &= ~BIT3;
//     }

//     __bic_SR_register_on_exit(LPM0_bits);
// }

