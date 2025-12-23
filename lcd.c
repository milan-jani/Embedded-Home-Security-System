#include <msp430f5529.h>
#include <string.h>

// LCD I2C defines
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

void init_i2c(void)
{
    P3SEL |= BIT0 | BIT1;          // SDA=P3.0, SCL=P3.1

    UCB0CTL1 |= UCSWRST;
    UCB0CTL0 = UCMST | UCMODE_3 | UCSYNC; // I2C master
    UCB0CTL1 = UCSSEL_2 | UCSWRST;        // SMCLK
    UCB0BR0 = 10;                          // ~100kHz @ 1MHz
    UCB0BR1 = 0;
    UCB0I2CSA = 0x27;                     // LCD I2C address (try 0x3F if doesn't work)
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

void lcd_cmd(unsigned char cmd)
{
    unsigned char data;

    // Upper nibble
    data = (cmd & 0xF0) | LCD_BACKLIGHT;
    i2c_write(data | LCD_ENABLE);
    __delay_cycles(100);
    i2c_write(data);
    __delay_cycles(100);

    // Lower nibble
    data = ((cmd << 4) & 0xF0) | LCD_BACKLIGHT;
    i2c_write(data | LCD_ENABLE);
    __delay_cycles(100);
    i2c_write(data);

    __delay_cycles(2000);
}

void lcd_data(unsigned char data)
{
    unsigned char d;

    // Upper nibble
    d = (data & 0xF0) | LCD_BACKLIGHT | LCD_RS;
    i2c_write(d | LCD_ENABLE);
    __delay_cycles(100);
    i2c_write(d);
    __delay_cycles(100);

    // Lower nibble
    d = ((data << 4) & 0xF0) | LCD_BACKLIGHT | LCD_RS;
    i2c_write(d | LCD_ENABLE);
    __delay_cycles(100);
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
    __delay_cycles(2000);
}

void lcd_print(const char *s, unsigned char crs)
{
    lcd_cmd(crs);   // Set cursor position
    int i=0;
    char f=0;
    for (i=0; i<strlen(s); i++) {
        lcd_data(s[i]);
        if(i>15 && !f){
            lcd_cmd(0xC0);  // Move to line 2
            f=1;
        } 
    }
    __delay_cycles(2000000);
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
