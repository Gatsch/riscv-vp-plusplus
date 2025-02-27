#ifndef RISCV_VP_DS1307_H
#define RISCV_VP_DS1307_H

#include <stdint.h>
#include <fstream>
#include "time.h"
#include <ctime>

#include "i2c_if.h"

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


class DS1307 : public I2C_Device_IF {
    uint8_t registers[64];
    uint8_t reg_pointer;
    uint8_t start_signal;

   protected:
    /* I2C_Device_IF implementation */
    bool start() override;
    bool write(uint8_t data) override;
    bool read(uint8_t &data) override;
    bool stop() override;

    // internal functions for handling time and date
    struct tm get_local_date_time();
    struct tm get_date_time();
    struct tm diff_date_time(struct tm t1, struct tm t2);
    bool save_diff_date_time(struct tm& diff);
    bool get_diff_date_time(struct tm& diff);
    void update_date_time(struct tm diff, uint8_t mode_12h);

   public:
    DS1307();

}

#endif /* RISCV_VP_DS1307_H */