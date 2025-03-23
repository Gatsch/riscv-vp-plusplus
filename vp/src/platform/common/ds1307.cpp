#include "ds1307.h"
#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>

DS1307::DS1307() {
    reg_pointer = 0;
    start_signal = NOT_BUSY;
    for (int i = 0; i < 64; i++) {
        registers[i] = 0;
    }
    struct tm stopped_t;
    long long diff;
    uint8_t mode_12h;
    //this->reset_diff(); // ONLY FOR TESTING REMOVE FOR FINAL VERSION
    if (!this->load_mode12h(mode_12h, DS1307_MODE_FILE)) {
        mode_12h = 0;
        printf("Generating file %s\n", DS1307_MODE_FILE);
        this->save_mode12h(mode_12h, DS1307_MODE_FILE); // generate file
    }
    if (!this->load_date_time(stopped_t, DATE_TIME_HALT_VAL)) {
        stopped_t = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,""};
        printf("Generating file %s\n", DATE_TIME_HALT_VAL);
        this->save_date_time(stopped_t, DATE_TIME_HALT_VAL); // generate file
    }
    if (stopped_t.tm_sec == -1 && stopped_t.tm_hour == -1 && stopped_t.tm_min == -1) { // time is not stopped
        if( !this->load_diff(diff, DIFF_DATE_TIME_FILE)){
            diff = 0;
            printf("Generating file %s\n", DIFF_DATE_TIME_FILE);
            this->save_diff(diff, DIFF_DATE_TIME_FILE);

        }
        this->update_date_time(diff, mode_12h, 0);

    } else { // time stopped calculate current diff and set CH bit to 1
        diff = this->diff_date_time(stopped_t, this->get_local_date_time());
        this->save_diff(diff, DIFF_DATE_TIME_FILE);
        this->update_date_time(diff, mode_12h, 1);
    }
}

bool DS1307::start() {
    start_signal = START_RECEIVED;
    long long diff;
    // load current diff time and date from file and update registers
    if (!(registers[DS1307_ADRESS_SECONDS] & DS1307_BIT_CH_MASK)) {  // Clock is halted, do not update time
        this->load_diff(diff, DIFF_DATE_TIME_FILE);
        uint8_t mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
        uint8_t CH_bit = (registers[DS1307_ADRESS_SECONDS] & DS1307_BIT_CH_MASK) >> 7;
        this->update_date_time(diff, mode_12h, CH_bit);
        struct tm stopped_t = {-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,""}; // time is not stopped
        this->save_date_time(stopped_t, DATE_TIME_HALT_VAL);
    } else {
        struct tm stopped_t = this->get_date_time();
        this->save_date_time(stopped_t, DATE_TIME_HALT_VAL);
    }
    return true;
}

bool DS1307::write(uint8_t data) {
    if (start_signal == START_RECEIVED) {
        reg_pointer = data;
        start_signal = BUSY;
    } else if (start_signal == BUSY) {
        registers[reg_pointer] = data;
        reg_pointer++;
    } else {
        return false;
    }
    return true;
}

bool DS1307::read(uint8_t &data) {
    if (start_signal == START_RECEIVED) {
        data = registers[reg_pointer];
        reg_pointer++;
        return true;
    }
    return false;
}

bool DS1307::stop() {
    start_signal = NOT_BUSY;
    // update time diff
    struct tm set_time = this->get_date_time();
    struct tm  local_time = this->get_local_date_time();
    long long diff = this->diff_date_time(set_time, local_time);
    // save current diff and hour mode
    this->save_diff(diff, DIFF_DATE_TIME_FILE);
    uint8_t mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    this->save_mode12h(mode_12h, DS1307_MODE_FILE);
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
    // use correct time format according to struct tm specification
    current.tm_wday = ((registers[DS1307_ADRESS_DAY] & 0x07) == 7) ? 0 : (registers[DS1307_ADRESS_DAY] & 0x07) == 7;
    current.tm_mday = (registers[DS1307_ADRESS_DATE] & 0x0F) + 10 * ((registers[DS1307_ADRESS_DATE] & 0x30) >> 4);
    current.tm_mon = (registers[DS1307_ADRESS_MONTH] & 0x0F) + 10 * ((registers[DS1307_ADRESS_MONTH] & 0x10) >> 4) - 1;
    current.tm_year = (registers[DS1307_ADRESS_YEAR] & 0x0F) + 10 * ((registers[DS1307_ADRESS_YEAR] & 0xF0) >> 4) + 100;

    return current;
}

std::time_t DS1307::convert_tm_to_seconds(struct tm date_time) {
    // Convert tm to time_t (seconds since epoch)
    std::time_t time = mktime(&date_time); //timegm(&date_time);
    // Convert time_t to system_clock::time_point
    return time;
}

long long DS1307::diff_date_time(struct tm t1, struct tm t2) {
    // Convert both tm structures to time_points
    std::time_t time1 = convert_tm_to_seconds(t1);
    std::time_t time2 = convert_tm_to_seconds(t2);

    return time1 - time2;
}

bool DS1307::save_diff(long long& diff, const char* filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(&diff), sizeof(long long));
    outFile.close();
    return true;
}

bool DS1307::load_diff(long long& diff, const char* filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return false;
    }
    inFile.read(reinterpret_cast<char*>(&diff), sizeof(long long));
    inFile.close();
    return true;
}

bool DS1307::save_mode12h(uint8_t& mode, const char* filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(&mode), sizeof(uint8_t));
    outFile.close();
    return true;
}

bool DS1307::load_mode12h(uint8_t& mode, const char* filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return false;
    }
    inFile.read(reinterpret_cast<char*>(&mode), sizeof(uint8_t));
    inFile.close();
    return true;
}

bool DS1307::save_date_time(struct tm& date_time, const char* filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(&date_time), sizeof(tm));
    outFile.close();
    return true;
}

bool DS1307::load_date_time(struct tm& date_time, const char* filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return false;
    }
    inFile.read(reinterpret_cast<char*>(&date_time), sizeof(tm));
    inFile.close();
    return true;
}

/* update time and date saved in registers according to set time diff and 12 or 24h mode */
void DS1307::update_date_time(long long diff, uint8_t mode_12h, uint8_t CH_bit) {
    //struct tm current = this->get_date_time();
    std::time_t current = this->DS1307::convert_tm_to_seconds(this->get_local_date_time());
    std::time_t new_datetime = current + diff;

    struct tm ds1307_new_reg_vals;
    memcpy(&ds1307_new_reg_vals, localtime(&new_datetime), sizeof (struct tm));

    // adjust values to fit format of ds1307
    if (ds1307_new_reg_vals.tm_wday == 0) {ds1307_new_reg_vals.tm_wday = 7;}
    ds1307_new_reg_vals.tm_mon += 1;
    ds1307_new_reg_vals.tm_year -= 100;

    registers[DS1307_ADRESS_SECONDS] = (CH_bit << 7) + (((ds1307_new_reg_vals.tm_sec / 10) << 4) & 0x70) + (ds1307_new_reg_vals.tm_sec % 10);
    registers[DS1307_ADRESS_MINUTES] = (0 << 7) + (((ds1307_new_reg_vals.tm_min / 10) << 4) & 0x70) + (ds1307_new_reg_vals.tm_min % 10);
    if (mode_12h) {
        uint8_t pm = 0;
        uint8_t bit_mask_10h = 0x30;
        if (ds1307_new_reg_vals.tm_hour > 12) {
            pm = 1;
            ds1307_new_reg_vals.tm_hour -= 12;
            bit_mask_10h = 0x10; // adjust 10h register bit mask
        }
        registers[DS1307_ADRESS_HOURS] =   (0 << 7) + (1 << 6) + (pm << 5) +
                                           (((ds1307_new_reg_vals.tm_hour / 10) << 4) & bit_mask_10h) + (ds1307_new_reg_vals.tm_hour % 10);
    } else {
        registers[DS1307_ADRESS_HOURS] =   (0 << 7) + (((ds1307_new_reg_vals.tm_hour / 10) << 4) & 0x30) + (ds1307_new_reg_vals.tm_hour % 10);
    }
    registers[DS1307_ADRESS_DAY] = (ds1307_new_reg_vals.tm_wday) & 0x07;
    registers[DS1307_ADRESS_DATE] = (0 << 7) + (((ds1307_new_reg_vals.tm_mday / 10) << 4) & 0x30) + (ds1307_new_reg_vals.tm_mday % 10);
    registers[DS1307_ADRESS_MONTH] = (0 << 7) + ((((ds1307_new_reg_vals.tm_mon) / 10) << 4) & 0x10) + ((ds1307_new_reg_vals.tm_mon) % 10);
    registers[DS1307_ADRESS_YEAR] = ((((ds1307_new_reg_vals.tm_year) / 10) << 4) & 0xF0) + ((ds1307_new_reg_vals.tm_year) % 10);
    return;
}

struct tm DS1307::get_local_date_time() {
    time_t now = time(nullptr);
    struct tm  current_date_time;
    current_date_time = *localtime(&now);
    return current_date_time;
}

void DS1307::reset_diff() {
    long long diff = 0;
    this->save_diff(diff, DIFF_DATE_TIME_FILE); // initialize to 0 difference
}


