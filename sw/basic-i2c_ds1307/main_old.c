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

void i2c_irq_handler() {
	printf("I2C: INTERRUPT HANDLER\n");
}

void i2c_start(uint8_t address) {
    // Write slave address with write bit (0)
    *I2C_TXR = (address << 1) | 0;
    *I2C_CR = (1 << 7) | (1 << 4);  // Generate start condition and write

}

void i2c_stop() {
    // Generate stop condition
    *I2C_CR = (1 << 6);

}
void i2c_write(uint8_t address, uint8_t data) {
    
    // Wait for TIP (Transfer In Progress) to be cleared
    while (*I2C_SR & (1 << 1));

    // Write data to slave
    *I2C_TXR = data;
    *I2C_CR = (1 << 4);  // Write

    // Wait for TIP to be cleared
    while (*I2C_SR & (1 << 1));

}

uint8_t i2c_read(uint8_t address) {
    uint8_t data;

    // Write slave address with read bit (1)
    *I2C_TXR = (address << 1) | 1;
    *I2C_CR = (1 << 7) | (1 << 4);  // Generate start condition and read

    // Wait for TIP to be cleared
    while (*I2C_SR & (1 << 1));

    // Read data from slave
    *I2C_CR = (1 << 5);  // Read

    // Wait for TIP to be cleared
    while (*I2C_SR & (1 << 1));

    data = *I2C_TXR;  // Read data from slave

    return data;
}

int main() {
    register_interrupt_handler(50, i2c_irq_handler);
     printf("enable I2C controller\n");
    *I2C_CTR = 0x80;  // Enable I2C controller

    printf("Write\n");
    i2c_start(0x68);
    // Write data to slave
    //i2c_write(0x68, 0x02);  // Example: Write 0x02 to slave address 0x68
    i2c_write(0x68, 0x00); 
    i2c_stop();

    /*
    i2c_start(0x68);
    printf("Write\n");
    // Write data to slave
    i2c_write(0x68, 0x00);  // Example: Write 0x01 to slave address 0x68
    i2c_stop();
    */
    printf("Read\n");
    // Read data from slave
    int i = 0;
    for (int i = 0; i < 8; i++) {
    	uint8_t data = i2c_read(0x68);
    	printf("Received data: %d\n", data);
    	registers[i] = data;
    }
    //printf("Received data: %02x\n", data);
    
    print_date_time();
    

    return 0;
}
/*
int main() {
	register_interrupt_handler(50, i2c_irq_handler);
	printf("enable I2C controller\n");
	*I2C_CTR = 0x80;	// enable I2C controller
	printf("write slave address\n");
	*I2C_RXR_TXR = 0x68 << 15 | 1;	// write slave address
	printf("generate start condition\n");
	*I2C_CR_SR = 0x80 | (1 << 4);	// generate start condition
	printf("wait for TIP to be cleared\n");
	while (!(*I2C_CR_SR & 0x02));	// wait for TIP to be cleared
	printf("read and stop condition\n");
	*I2C_CR_SR = (1 << 5) | (1 << 6); //read and stop condition
	uint8_t data = 0x00;
	printf("wait for TIP to be cleared\n");
	while (!(*I2C_CR_SR & 0x02));	// wait for TIP to be cleared
	printf("read data from slave\n");
	data = *I2C_RXR_TXR;	// read data from slave

	printf("%d%d\n", data & 0x70, data & 0x0F);
	
	printf("test ");
	puts("a");

	return 0;
}
*/
