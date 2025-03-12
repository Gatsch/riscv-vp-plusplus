#include "stdint.h"
#include "stdio.h"

static volatile uint32_t * const I2C_CTR = (uint32_t * const)0x10030002;
static volatile uint32_t * const I2C_RXR_TXR = (uint32_t * const)0x44000003;
static volatile uint32_t * const I2C_CR_SR = (uint32_t * const)0x44000004;

int main() {
	*I2C_CTR = 0x80;	// enable I2C controller
	*I2C_RXR_TXR = 0x68 << 15 | 1;	// write slave address
	*I2C_CR_SR = 0x80 | (1 << 4);	// generate start condition
	while (!(*I2C_CR_SR & 0x02));	// wait for TIP to be cleared
	*I2C_CR_SR = (1 << 5) | (1 << 6); //read and stop condition
	uint8_t data = 0x00;
	while (!(*I2C_CR_SR & 0x02));	// wait for TIP to be cleared
	data = *I2C_RXR_TXR;	// read data from slave

	printf("%d%d\n", data & 0x70, data & 0x0F);

	return 0;
}
