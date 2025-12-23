/*
 * SECURITY SYSTEM - MSP430F5529LP
 * By Milan Jani
 * 
 * Features:
 * - IR Intrusion Detection
 * - Gas (MQ-6) Sensor
 * - Flame Sensor
 * - SOS Button
 * - UART Password Entry
 * - Servo Door Lock
 * - LCD Display
 */

#include <msp430f5529.h>

// ==================== LCD.C Functions ====================
extern void init_i2c(void);
extern void lcd_init(void);
extern void lcd_cmd(unsigned char cmd);
extern void lcd_print(const char *s, unsigned char crs);

// ==================== UART.C Functions & Variables ====================
extern void init_uart(void);
extern void handle_password_input(void);
extern volatile unsigned char uart_rx_flag;
extern volatile unsigned char pass_mode_active;

// ==================== SERVO.C Functions ====================
extern void init_servo_pwm(void);
extern void servo_open(void);
extern void servo_close(void);

// ==================== SENSORS.C Functions & Variables ====================
extern void init_gpio(void);
extern void init_timer(void);
extern void init_adc_mq6(void);
extern void check_gas_level(void);
extern void handle_gas_alert(void);
extern void handle_ir_intrusion(void);
extern void handle_flame_detect(void);
extern void buzzer_beep(void);
extern void buzzer_1sec(void);

extern volatile unsigned char ir_intrusion_flag;
extern volatile unsigned char gas_alert_flag;
extern volatile unsigned char sos_button_flag;
extern volatile unsigned char gas_check_flag;
extern volatile unsigned char flame_detect_flag;
extern volatile unsigned char fire_override_active;

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // Stop watchdog timer

    // Initialize all peripherals
    init_gpio();        // GPIO pins for sensors, buzzer
    init_i2c();         // I2C for LCD
    lcd_init();         // LCD initialization
    init_servo_pwm();   // Servo motor PWM - MOVE BEFORE UART
    init_uart();        // UART for password input
    init_adc_mq6();     // Gas sensor ADC
    init_timer();       // Timer for periodic gas check
    
    __bis_SR_register(GIE);   // Enable global interrupts

    // Startup display
    lcd_cmd(0x01);
    lcd_print("SECURITY SYSTEM", 0x80);
    lcd_print("By Milan Jani", 0xC0);
    __delay_cycles(2000000);
    lcd_cmd(0x01);
    
    // Test servo movement
    lcd_print("Testing Servo", 0x80);
    servo_open();
    __delay_cycles(2000000);
    servo_close();
    __delay_cycles(1000000);
    
    lcd_cmd(0x01);

    // Main loop
    while(1)
    {
        //lcd_cmd(0x01);
        // SOS Button (highest priority after fire)
        if(sos_button_flag)
        {
            buzzer_1sec();
            sos_button_flag = 0;
        }

        // Fire detection overrides everything
        if(flame_detect_flag)
        {
            handle_flame_detect();
            flame_detect_flag = 0;
        }

        // Normal operation (if no fire)
        if(!fire_override_active)
        {
            // Gas level monitoring
            if(gas_check_flag && !pass_mode_active)
            {
                check_gas_level();
                // lcd_cmd(0xC0);
                // lcd_print("PASS:", 0xC0);
                gas_check_flag = 0;
            }

            // UART password input
            if(uart_rx_flag)
            {
                handle_password_input();  
                uart_rx_flag = 0;
            }

            // Gas alert
            if(gas_alert_flag)
            {
                handle_gas_alert();
                gas_alert_flag = 0;
            }

            // IR intrusion detection
            if(ir_intrusion_flag)
            {
                handle_ir_intrusion();
                ir_intrusion_flag = 0;
            }
        }

        // Low power mode until next interrupt
        __bis_SR_register(LPM0_bits);
    }
}
