#include "ds1307.h"
#include <iostream>

DS1307::DS1307() {
    reg_pointer = 0;
    start_signal = 0;
    for (int i = 0; i < 64; i++) {
        registers[i] = 0;
    }
    //struct tm diff_init = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    //this->save_diff_date_time(diff_init); // initialize to 0 difference
    struct tm  init_time = this->get_local_date_time(); // initialize to current time
    // set registers
    // TODO: CH bit in the seconds register will be set to a 1. The clock can be halted
    //  whenever the timekeeping functions are not required (implement?)
    registers[DS1307_ADRESS_SECONDS] = (0 << 7) + (((init_time.tm_sec / 10) << 4) & 0x70) + (init_time.tm_sec % 10);
    registers[DS1307_ADRESS_MINUTES] = (0 << 7) + (((init_time.tm_min / 10) << 4) & 0x70) + (init_time.tm_min % 10);
    registers[DS1307_ADRESS_HOURS] =   (0 << 7) + (((init_time.tm_hour / 10) << 4) & 0x30) + (init_time.tm_hour % 10);
    registers[DS1307_ADRESS_DAY] = (init_time.tm_wday) & 0x07;
    registers[DS1307_ADRESS_DATE] = (0 << 7) + (((init_time.tm_mday / 10) << 4) & 0x30) + (init_time.tm_mday % 10);
    registers[DS1307_ADRESS_MONTH] = (0 << 7) + (((init_time.tm_mon / 10) << 4) & 0x10) + (init_time.tm_mon % 10);
    registers[DS1307_ADRESS_YEAR] = (((init_time.tm_year / 10) << 4) & 0xF0) + (init_time.tm_year % 10);
}

bool DS1307::start() {
    start_signal = 1;
    struct tm diff;
    // load current diff time and date from file and update registers
    this->get_diff_date_time(diff);
    uint8_t mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    this->update_date_time(diff, mode_12h);
    return true;
}

bool DS1307::write(uint8_t data) {
    if (start_signal) {
        reg_pointer = data;
        start_signal = 0;
    } else {
        registers[reg_pointer] = data;
        reg_pointer++;
    }
    return true;
}

bool DS1307::read(uint8_t &data) {
    data = registers[reg_pointer];
    reg_pointer++;
    return true;
}

bool DS1307::stop() {
    // update time diff
    // TODO only update after write? 
    struct tm set_time = this->get_date_time();
    struct tm  local_time = this->get_local_date_time();
    struct tm diff = this->diff_date_time(set_time, local_time);
    this->save_diff_date_time(diff);
    return true;
}

/* convert time and date information saved in registers into struct tm */
struct tm DS1307::get_date_time() {
    struct tm current;
    current.tm_sec = (registers[DS1307_ADRESS_SECONDS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_SECONDS] & 0x70) >> 4);
    current.tm_min = (registers[DS1307_ADRESS_MINUTES] & 0x0F) + 10 * ((registers[DS1307_ADRESS_MINUTES] & 0x70) >> 4);
    // check if rtc is set to 12h or 24h mode
    uint8_t hour_mode = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    if (hour_mode) { // 12h mode
        uint8_t pm = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_PM_AM_MASK) >> 5;
        current.tm_hour = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x10) >> 4)
                + pm*12;
    } else { // 24 h mode
        current.tm_hour = (registers[DS1307_ADRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADRESS_HOURS] & 0x30) >> 4);
    }
    // TODO: comply with ds1307 values or ctime values for representing days, months, years?
    current.tm_wday = (registers[DS1307_ADRESS_DAY] & 0x07);
    current.tm_mday = (registers[DS1307_ADRESS_DATE] & 0x0F) + 10 * ((registers[DS1307_ADRESS_DATE] & 0x30) >> 4);
    current.tm_mon = (registers[DS1307_ADRESS_MONTH] & 0x0F) + 10 * ((registers[DS1307_ADRESS_MONTH] & 0x10) >> 4);
    current.tm_year = (registers[DS1307_ADRESS_YEAR] & 0x0F) + 10 * ((registers[DS1307_ADRESS_YEAR] & 0xF0) >> 4);

    return current;
}

struct tm DS1307::diff_date_time(struct tm t1, struct tm t2) {
    struct tm diff;
    diff.tm_sec = t1.tm_sec - t2.tm_sec;
    diff.tm_min = t1.tm_min - t2.tm_min;
    diff.tm_hour = t1.tm_hour - t2.tm_hour;
    diff.tm_wday = t1.tm_wday - t2.tm_wday;
    diff.tm_mday = t1.tm_mday - t2.tm_mday;
    diff.tm_mon = t1.tm_mon - t2.tm_mon;
    diff.tm_year = t1.tm_year - t2.tm_year;
    return diff;
}

bool DS1307::save_diff_date_time(struct tm& diff) {
    // TODO: think about how to handle new initialization with previous struct still stored in file...
    std::ofstream outFile("date_time_diff", std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(&diff), sizeof(tm));
    outFile.close();
    return true;
}

bool DS1307::get_diff_date_time(struct tm& diff) {
    std::ifstream inFile("date_time_diff", std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return false;
    }
    inFile.read(reinterpret_cast<char*>(&diff), sizeof(tm));
    inFile.close();
    return true;
}

/* update time and date saved in registers according to set time diff and 12 or 24h mode */
void DS1307::update_date_time(struct tm diff, uint8_t mode_12h) {
    //struct tm current = this->get_date_time();
    struct tm  current = this->get_local_date_time();
    struct tm new_date_time;
    uint8_t overflow = 0;
    new_date_time.tm_sec = current.tm_sec + diff.tm_sec;
    if (new_date_time.tm_sec >= 60 ) {
        new_date_time.tm_sec -= 60;
        overflow = 1;
    }
    new_date_time.tm_min = current.tm_min + diff.tm_min + overflow;
    if (new_date_time.tm_min >= 60 ) {
        new_date_time.tm_min -= 60;
        overflow = 1;
    } else {
        overflow = 0;
    }
    new_date_time.tm_hour = current.tm_hour + diff.tm_hour + overflow;
    if (new_date_time.tm_hour >= 24 ) {
        new_date_time.tm_min -= 24;
        overflow = 1;
    } else {
        overflow = 0;
    }
    new_date_time.tm_wday = current.tm_wday + diff.tm_wday;
    if (new_date_time.tm_wday >= 8 ) {
        new_date_time.tm_wday -= 7;
        overflow = 1;
    } else {
        overflow = 0;
    }
    // TODO: Overflow handling
    new_date_time.tm_mday = current.tm_mday + diff.tm_mday;
    switch (current.tm_mon) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            if (new_date_time.tm_mday >= 32 ) {
                new_date_time.tm_mday -= 31;
                overflow = 1;
            } else {
                overflow = 0;
            }
            break;
        case 4: case 6: case 9: case 11:
            if (new_date_time.tm_mday >= 31 ) {
                new_date_time.tm_mday -= 30;
                overflow = 1;
            } else {
                overflow = 0;
            }
            break;
        case 2:
            if (new_date_time.tm_mday >= 29 ) {
                new_date_time.tm_mday -= 28;
                overflow = 1;
            } else {
                overflow = 0;
            }
            break;
    }
    new_date_time.tm_mon = current.tm_mon + diff.tm_mon + overflow;
    if (new_date_time.tm_mon >= 13 ) {
        new_date_time.tm_mon -= 12;
        overflow = 1;
    } else {
        overflow = 0;
    }
    new_date_time.tm_year = current.tm_year + diff.tm_year + overflow;


    registers[DS1307_ADRESS_SECONDS] = (0 << 7) + (((new_date_time.tm_sec / 10) << 4) & 0x70) + (new_date_time.tm_sec % 10);
    registers[DS1307_ADRESS_MINUTES] = (0 << 7) + (((new_date_time.tm_min / 10) << 4) & 0x70) + (new_date_time.tm_min % 10);
    if (mode_12h) {
        uint8_t pm = 0;
        uint8_t bit_mask_10h = 0x30;
        if (new_date_time.tm_hour > 12) {
            pm = 1;
            new_date_time.tm_hour -= 12;
            bit_mask_10h = 0x10; // adjust 10h register bit mask
        }
        registers[DS1307_ADRESS_HOURS] =   (0 << 7) + (1 << 6) + (pm << 5) + (((new_date_time.tm_hour / 10) << 4) & bit_mask_10h) + (new_date_time.tm_hour % 10);
    } else {
        registers[DS1307_ADRESS_HOURS] =   (0 << 7) + (((new_date_time.tm_hour / 10) << 4) & 0x30) + (new_date_time.tm_hour % 10);
    }
    registers[DS1307_ADRESS_DAY] = (new_date_time.tm_wday) & 0x07;
    registers[DS1307_ADRESS_DATE] = (0 << 7) + (((new_date_time.tm_mday / 10) << 4) & 0x30) + (new_date_time.tm_mday % 10);
    registers[DS1307_ADRESS_MONTH] = (0 << 7) + ((((new_date_time.tm_mon) / 10) << 4) & 0x10) + ((new_date_time.tm_mon) % 10);
    registers[DS1307_ADRESS_YEAR] = ((((new_date_time.tm_year) / 10) << 4) & 0xF0) + ((new_date_time.tm_year) % 10);

}

struct tm DS1307::get_local_date_time() {
    time_t now = time(nullptr);
    struct tm  current_date_time;
    current_date_time = *localtime(&now);
    // adjust values to fit with ds1307 specification
    current_date_time.tm_mon += 1;
    current_date_time.tm_year -= 100;
    return current_date_time;
}


