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

void wait_seconds_timer(int seconds) {
    // Simple delay loop for bare-metal environment
    for (int s = 0; s < seconds; s++) {
        for (volatile int i = 0; i < 1000000; i++);  // Adjust multiplier as needed
    }
}



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
    *I2C_CR = I2C_CR_IACK;
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
    *I2C_CR = I2C_CR_RD | (stop ? I2C_CR_STO | I2C_CR_ACK : 0);
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
    printf("\tStart\n");
    i2c_start(0x68, 0);
    // Write data to slave
    printf("\tWrite year register address to slave\n");
    i2c_write(DS1307_ADRESS_YEAR, 0); 
    printf("\tWrite year 2024 to year register and stop\n");
    i2c_write(0x24, 1); 
    

    
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

    // Test CH bit functionality
    printf("\n=== CH Bit Test ===\n");
    
    printf("Reading current time...\n");
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);  // Set register pointer to seconds
    
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    
    printf("Time before CH bit set:\n");
    print_date_time();
    
    // Save original seconds value (without CH bit)
    uint8_t original_seconds = registers[DS1307_ADRESS_SECONDS] & 0x7F;  // Clear CH bit
    
    printf("\nSetting CH bit to halt clock...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_SECONDS, 0);  // Point to seconds register
    i2c_write(original_seconds | DS1307_BIT_CH_MASK, 1);  // Set CH bit

    printf("Waiting for 2 seconds...\n");
    wait_seconds_timer(2);  
    printf("Reading time after 2 seconds...\n");
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);  // Set register pointer to seconds
    
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    
    printf("Time after 2 seconds with CH bit set:\n");
    print_date_time();
    // verify that date time has not change compared to original_seconds
    uint8_t new_seconds = registers[DS1307_ADRESS_SECONDS] & 0x7F;
    if (new_seconds == original_seconds) {
        printf("Clock is halted correctly, seconds remain unchanged\n");
    } else {
        printf("Error: Clock did not halt as expected.\n");
    }

    printf("\nClearing CH bit to restart clock...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_SECONDS, 0);  // Point to seconds register
    i2c_write(original_seconds, 1);  // Clear CH bit (write original seconds without CH bit)

    printf("Waiting 2 seconds to verify clock restart...\n");
    wait_seconds_timer(2);
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);
    
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    printf("Time after clearing CH bit:\n");
    print_date_time();

    uint8_t final_seconds = registers[DS1307_ADRESS_SECONDS] & 0x7F;
    
    if (final_seconds != original_seconds) {
        printf("CLOCK RESTART TEST PASSED: Clock is running again!\n");
    } else {
        printf("CLOCK RESTART TEST FAILED: Clock may still be halted!\n");
    }
    
    printf("\n=== CH Bit Test Complete ===\n\n");

    // Test 12h/24h mode functionality
    printf("\n=== 12h/24h Mode Test ===\n");
    
    printf("Setting time to 14:30 (2:30 PM) in 24h mode...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_SECONDS, 0);
    i2c_write(0x00, 0);  // 00 seconds, CH bit clear
    i2c_write(0x30, 0);  // 30 minutes
    i2c_write(0x14, 1);  // 14 hours (24h mode, bit 6 = 0)

    // Read back and display
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    printf("Time in 24h mode:\n");
    print_date_time();
    
    uint8_t hour_24h = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x30) >> 4);
    uint8_t mode_check = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    
    printf("\nConverting to 12h mode (2:30 PM)...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_HOURS, 0);
    // 12h mode: bit 6 = 1, PM bit 5 = 1, hour = 02
    i2c_write(0x62, 1);  // 0110 0010 = 12h mode + PM + 02

    
    // Read back and display
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    
    printf("Time in 12h mode:\n");
    print_date_time();
    
    uint8_t hour_mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    uint8_t pm_bit = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_PM_AM_MASK) >> 5;
    uint8_t hour_12h = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x10) >> 4);
    
    if (hour_mode_12h && pm_bit && hour_12h == 2) {
        printf("12h MODE SET CORRECTLY: 2:30 PM\n");
    } else {
        printf("12h MODE FAILED: Mode=%d, PM=%d, Hour=%d\n", hour_mode_12h, pm_bit, hour_12h);
    }
    
    printf("\n Setting time to 10:15 AM in 12h mode...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_MINUTES, 0);
    i2c_write(0x15, 0);  // 15 minutes
    i2c_write(0x50, 1);  // 0101 0000 = 12h mode + AM + 10

    // Read back and display
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    printf("Time in 12h AM mode:\n");
    print_date_time();
    
    uint8_t am_pm_bit = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_PM_AM_MASK) >> 5;
    uint8_t hour_am = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x10) >> 4);
    
    if (!am_pm_bit && hour_am == 10) {
        printf("12h AM MODE SET CORRECTLY: 10:15 AM\n");
    } else {
        printf("12h AM MODE FAILED: AM/PM bit=%d, Hour=%d\n", am_pm_bit, hour_am);
    }

    printf("\nConverting back to 24h mode (10:15)...\n");
    i2c_start(0x68, 0);
    i2c_write(DS1307_ADRESS_HOURS, 0);
    i2c_write(0x10, 1);  // 24h mode, 10 hours
    
    // Read back and display
    i2c_start(0x68, 0);
    i2c_write(0x00, 1);
    i2c_start(0x68, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t data = i2c_read(i == 7);
        registers[i] = data;
    }

    printf("Final time back in 24h mode:\n");
    print_date_time();
    
    uint8_t final_mode = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    uint8_t final_hour = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x30) >> 4);
    
    if (!final_mode && final_hour == 10) {
        printf("BACK TO 24h MODE CORRECTLY: 10:15\n");
    } else {
        printf("24h MODE CONVERSION FAILED: Mode=%d, Hour=%d\n", final_mode, final_hour);
    }
    
    printf("\n=== 12h/24h Mode Test Complete ===\n\n");

    return 0;
}

