#include "stdint.h"
#include "stdio.h"
#include "irq.h"



#define DS1307_ADRESS_SECONDS 0x00
#define DS1307_ADRESS_MINUTES 0x01
#define DS1307_ADRESS_HOURS 0x02
#define DS1307_ADRESS_DAY 0x03
#define DS1307_ADRESS_DATE 0x04
#define DS1307_ADRESS_MONTH 0x05
#define DS1307_ADRESS_YEAR 0x06
#define DS1307_ADRESS_CONTROL 0x07
#define DS1307_ADRESS_RAM_BEGIN 0x08
#define DS1307_ADRESS_RAM_END 0x3F

#define DS1307_BIT_12_24_MASK 0x40
#define DS1307_BIT_PM_AM_MASK 0x20
#define DS1307_BIT_CH_MASK 0x80

static volatile uint32_t * const I2C_CTR = (uint32_t * const)0x10030008;
static volatile uint32_t * const I2C_RXR = (uint32_t * const)0x1003000C;
static volatile uint32_t * const I2C_TXR = (uint32_t * const)0x1003000C;
static volatile uint32_t * const I2C_CR = (uint32_t * const)0x10030010;
static volatile uint32_t * const I2C_SR = (uint32_t * const)0x10030010;

uint8_t registers[64];

void print_date_time() {
    printf("SEC: %d\n", (registers[DS1307_ADRESS_SECONDS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_SECONDS] & 0x70) >> 4));
  
    printf("MIN: %d\n",(registers[DS1307_ADRESS_MINUTES] & 0x0F) + 10 * ((registers[DS1307_ADRESS_MINUTES] & 0x70) >> 4));
    // check if rtc is set to 12h or 24h mode

    uint8_t hour_mode = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    if (hour_mode) { // 12h mode
        uint8_t pm = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_PM_AM_MASK) >> 5;
        printf("H: %d\n",(registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x10) >> 4)
                          + pm*12);
    } else { // 24 h mode
        printf("H: %d\n",(registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x30) >> 4));
    }

    // use correct time format according to struct tm specification
    printf("WDAY: %d\n",((registers[DS1307_ADRESS_DAY] & 0x07)));

    printf("MDAY: %d\n",(registers[DS1307_ADRESS_DATE] & 0x0F) + 10 * ((registers[DS1307_ADRESS_DATE] & 0x30) >> 4));

    printf("MON: %d\n",(registers[DS1307_ADRESS_MONTH] & 0x0F) + 10 * ((registers[DS1307_ADRESS_MONTH] & 0x10) >> 4));

    printf("Y: %d\n",(registers[DS1307_ADRESS_YEAR] & 0x0F) + 10 * ((registers[DS1307_ADRESS_YEAR] & 0xF0) >> 4));

}

#define I2C_CTR_EN (1 << 7)
#define I2C_CTR_IEN (1 << 6)
#define I2C_TX_ADDR (0xFF ^ 1)
#define I2C_TX_WR 1
#define I2C_CR_STA (1 << 7)
#define I2C_CR_STO (1 << 6)
#define I2C_CR_RD (1 << 5)
#define I2C_CR_WR (1 << 4)
#define I2C_CR_ACK (1 << 3)
#define I2C_CR_IACK (1)
#define I2C_SR_RXACK (1 << 7)
#define I2C_SR_BUSY (1 << 6)
#define I2C_SR_AL (1 << 5)
#define I2C_SR_TIP (1 << 1)
#define I2C_SR_IF (1)

void i2c_irq_handler() {
	printf("I2C: INTERRUPT HANDLER\n");
}

void i2c_enable(uint8_t enable) {
    if (enable) {
        *I2C_CTR |= I2C_CTR_EN;
    } else {
        *I2C_CTR &= ~I2C_CTR_EN;
    }
}

void i2c_interruptEnable(uint8_t enable) {
    if (enable) {
        *I2C_CTR |= I2C_CTR_IEN;
    } else {
        *I2C_CTR &= ~I2C_CTR_IEN;
    }
}

void i2c_start(uint8_t address, uint8_t rnw) {
    *I2C_TXR = (address << 1) | rnw;
    *I2C_CR = I2C_CR_STA | I2C_CR_WR;
    while (*I2C_SR & I2C_SR_TIP);
}

uint8_t i2c_read(uint8_t stop) {
    *I2C_CR = I2C_CR_RD | (stop ? I2C_CR_STO : 0);
    while (*I2C_SR & I2C_SR_TIP);
    return *I2C_RXR;
}

void i2c_write(uint8_t data, uint8_t stop) {
    *I2C_TXR = data;
    *I2C_CR = I2C_CR_WR | (stop ? I2C_CR_STO : 0);
    while (*I2C_SR & I2C_SR_TIP);
}


int main() {
    register_interrupt_handler(50, i2c_irq_handler);
    printf("enable I2C controller and Interrupts\n");
    i2c_enable(1);
    i2c_interruptEnable(1);
		
    printf("Write 1:\n");
    printf("\tStart");
    i2c_start(0x68, 0);
    // Write data to slave
    printf("\tWrite year register address to slave\n");
    i2c_write(DS1307_ADRESS_YEAR, 0); 
    printf("\tWrite 0 to year register and stop\n");
    i2c_write(0x25, 1); 
    
    
    printf("Write 2:\n");
    printf("\tStart");
    i2c_start(0x68, 0);
    // Write data to slave
    printf("\tWrite 0 address to slave and stop\n");
    i2c_write(0x00, 1);


    printf("Read 1:\n");
    // Read data from slave
    int i = 0;
    printf("\tStart");
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        printf("\tRead data %d from slave%s\n", i, i == 7 ? " and stop" : "");
    	uint8_t data = i2c_read(i == 7);
    	printf("\tReceived data: %d\n", data);
    	registers[i] = data;
    }
    print_date_time();
    return 0;
}