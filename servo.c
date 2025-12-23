#include <msp430f5529.h>

// Timer A1 PWM initialization for SG90 servo on P2.0 (TA1.1)
void init_servo_pwm(void)
{
    // Configure P2.0 as TA1.1 output (servo control pin)
    // IMPORTANT: For F5529, TA1.1 is on P2.0, NOT P2.4!
    P2DIR |= BIT0;
    P2SEL |= BIT0;
    P2OUT &= ~BIT0;  // Start LOW
    
    // Timer A1 setup for 50Hz PWM (20ms period)
    // SG90 expects 50Hz: 1ms=0°, 1.5ms=90°, 2ms=180°
    TA1CCR0 = 20000 - 1;          // 20ms period @ 1MHz SMCLK (20000 counts)
    TA1CCTL1 = OUTMOD_7;          // Reset/Set mode
    TA1CCR1 = 1500;               // Default position (neutral ~1.5ms = 90°)
    TA1CTL = TASSEL_2 | MC_1 | TACLR;     // SMCLK, Up mode, clear
    
    __delay_cycles(500000);       // Wait 0.5 sec for servo to reach position
}

void servo_open(void)
{
    // SG90: 2ms pulse = 180° (or max rotation)
    TA1CCR1 = 2000;   // ~2.0 ms → Full open
    __delay_cycles(1000000);  // Wait 1 sec for servo to move
}

void servo_close(void)
{
    // SG90: 1ms pulse = 0° (or min rotation)
    TA1CCR1 = 1000;   // ~1.0 ms → Full close
    __delay_cycles(1000000);  // Wait 1 sec for servo to move
}
