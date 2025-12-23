#include <msp430f5529.h>
#include <string.h>

#define PASSWORD "1234"
#define PASS_LEN 4

// External function declarations (from other .c files)
extern void lcd_cmd(unsigned char cmd);
extern void lcd_data(unsigned char data);
extern void lcd_print(const char *s, unsigned char crs);
extern void servo_open(void);
extern void servo_close(void);
extern void buzzer_beep(void);

// Global variables
volatile unsigned char uart_rx_flag = 0;
volatile char uart_rx_char;
volatile unsigned char pass_mode_active = 0;
char pass_buf[5];
unsigned char pass_index = 0;

void init_uart(void)
{
    P4SEL |= BIT4 | BIT5;     // P4.4=TX, P4.5=RX

    UCA1CTL1 |= UCSWRST;
    UCA1CTL1 = UCSSEL__SMCLK | UCSWRST;   // SMCLK = 1 MHz

    // Baud rate settings for 9600 @ 1MHz SMCLK
    // More reliable settings with proper modulation
    UCA1BR0 = 6;       // 1MHz / 16 / 9600 = 6.51 (oversampling mode)
    UCA1BR1 = 0;
    UCA1MCTL = UCBRS_0 | UCBRF_8 | UCOS16;  // Modulation + oversampling

    UCA1CTL1 &= ~UCSWRST;
    UCA1IE  |= UCRXIE;  // Enable RX interrupt
    
    __delay_cycles(10000);  // Let UART stabilize
}

#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    // Wait for RX to complete fully
    while(!(UCA1IFG & UCRXIFG));
    
    if(UCA1IFG & UCRXIFG)
    {
        uart_rx_char = UCA1RXBUF;
        
        // Validate: Only accept digits 0-9
        if(uart_rx_char >= '0' && uart_rx_char <= '9')
        {
           // lcd_cmd(0x01);
           //KYUUUUUUUUUUUUUU????
            lcd_data(uart_rx_char);
            uart_rx_flag = 1;
        }
        
        UCA1IFG &= ~UCRXIFG;  // Clear RX flag
        __bic_SR_register_on_exit(LPM0_bits);
    }
}

void lcd_show_pass_char(char c)
{
    // Show password digit at current cursor position
    // Don't move cursor - let it auto-increment
    lcd_data(c);
}

void print_pass(char pass_buf[])
{
    // Debug: Print entire password buffer on LCD
    int i = 0;
    while(i < PASS_LEN)
    {
        lcd_data(pass_buf[i]);
        i++;
    }
}

void green_led(void)
{
    P4DIR |= BIT7;   // P4.7 
    
    // Blink 4 times in 2 seconds
    int i=0;
    for(i = 0; i < 4; i++)
    {
        P4OUT |= BIT7;   // ON
        __delay_cycles(250000);  // 0.25 sec
        P4OUT &= ~BIT7;  // OFF
        __delay_cycles(250000);  // 0.25 sec
    }
}

void handle_password_input(void)
{
    pass_mode_active = 1;

    // Already validated in ISR - only digits reach here
    buzzer_beep();
        
        // First digit? Clear screen and print "PASS:"
        if(pass_index == 0)
        {
            lcd_cmd(0x01);  // Clear entire LCD
            __delay_cycles(2000);
           // lcd_cmd(0x80);  // Line 1
            // lcd_data('E');
            // lcd_data('n');
            // lcd_data('t');
            // lcd_data('e');
            // lcd_data('r');
            // lcd_data(' ');
            // lcd_data('P');
            // lcd_data('a');
            // lcd_data('s');
            // lcd_data('s');
            lcd_print("Enter pass",0x80);
            // lcd_cmd(0xC0);  // Line 2
            // lcd_data('P');
            // lcd_data('A');
            // lcd_data('S');
            // lcd_data('S');
            // lcd_data(':');
            lcd_print("Pass",0xC0);

        }
        
        pass_buf[pass_index] = uart_rx_char;
        //lcd_show_pass_char(uart_rx_char);  // Print at current cursor position
        //lcd_data(uart_rx_char);
        pass_index++;

        if(pass_index >= PASS_LEN)   // All 4 digits received
        {
            pass_buf[PASS_LEN] = '\0';  // NULL terminate
            
            __delay_cycles(1000000);  // Wait 1 sec to see entered password
            
            lcd_cmd(0x01);   // Clear LCD
            __delay_cycles(5000);

            // Compare password character by character
            unsigned char match = 1;
            int i;
            for(i = 0; i < PASS_LEN; i++)
            {
                if(pass_buf[i] != PASSWORD[i])
                {
                    match = 0;
                    break;
                }
            }

            if(match)  // Password correct
            {
                 
                lcd_print("Door Unlocked",0x80);
               green_led();
                lcd_print(pass_buf, 0xC0);
                __delay_cycles(2000000);
                servo_open();
                __delay_cycles(3000000);  // Door open 3 sec
                servo_close();
                lcd_cmd(0x01);
                
            }
            else  // Wrong password
            {
                lcd_print("Wrong Pass",0x80);
                
                lcd_print("Try again !!",0xC0);
                
             __delay_cycles(2000000);
                servo_close();  // Keep door locked
                lcd_cmd(0x01);
            }

            // Reset for next attempt
            pass_index = 0;
            pass_mode_active = 0;
            
            // Clear buffer
            for(i = 0; i < 5; i++)
            {
                pass_buf[i] = 0;
            }
        }
}
