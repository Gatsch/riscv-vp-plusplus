#include "ds1307.h"
#include <iostream>

DS1307::DS1307() {
    reg_pointer = 0;
    start_signal = NOT_BUSY;

    if(!load_state(registers, DS1307_STATE_FILE)) {
        printf("Generating file %s\n", DS1307_STATE_FILE);
        for (int i = 0; i < 64; i++) {
            registers[i] = 0;
        }
        save_state(registers, DS1307_STATE_FILE);
    }
    long long diff;
    uint8_t mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
    uint8_t CH_bit = (registers[DS1307_ADRESS_SECONDS] & DS1307_BIT_CH_MASK) >> 7;
    //reset_rtc(); // ONLY FOR TESTING REMOVE FOR FINAL VERSION
    if (!CH_bit) { // if CH bit is not set clock is not stopped
        if( !load_diff(diff, DIFF_DATE_TIME_FILE)){
            diff = 0;
            printf("Generating file %s\n", DIFF_DATE_TIME_FILE);
            save_diff(diff, DIFF_DATE_TIME_FILE);
        }
        update_date_time(diff, mode_12h, CH_bit);

    } else { // time stopped calculate current diff and set CH bit to 1
        diff = diff_date_time(get_date_time(), get_local_date_time());
        save_diff(diff, DIFF_DATE_TIME_FILE);
        update_date_time(diff, mode_12h, CH_bit);
    }
}

bool DS1307::start() {
    //printf("START SIGNAL RECEIVED \n");
    start_signal = START_RECEIVED;
    long long diff;
    // load current diff time and date from file and update registers
    if (!(registers[DS1307_ADRESS_SECONDS] & DS1307_BIT_CH_MASK)) {  // Clock is halted, do not update time
        load_diff(diff, DIFF_DATE_TIME_FILE);
        uint8_t mode_12h = (registers[DS1307_ADRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
        uint8_t CH_bit = (registers[DS1307_ADRESS_SECONDS] & DS1307_BIT_CH_MASK) >> 7;
        update_date_time(diff, mode_12h, CH_bit);
    }
    return true;
}

bool DS1307::write(uint8_t data) {
    if (start_signal == START_RECEIVED) {
    	//printf("SET REG POINTER: %d \n", data);
        reg_pointer = data;
        start_signal = BUSY;
    } else if (start_signal == BUSY) { // maybe -1 and -1 when writing month and day for simulation in rv64 to work?
        //printf("Write to %d with data: %d \n",reg_pointer, data);
        registers[reg_pointer] = data;
        reg_pointer++;
    } else {
        //printf("NO WRITE OF DATA %d \n",data);
        return false;
    }
    return true;
}

bool DS1307::read(uint8_t &data) {
    if (start_signal == START_RECEIVED) {
        data = registers[reg_pointer];
        //printf("READ FROM %d DATA: %d \n",reg_pointer, data);
        reg_pointer++;
        return true;
    }
    return false;
}

bool DS1307::stop() {
    //printf("STOP SIGNAL RECEIVED \n");
    start_signal = NOT_BUSY;
    // update time diff
    struct tm set_time = get_date_time();
    struct tm  local_time = get_local_date_time();
    long long diff = diff_date_time(set_time, local_time);
    // save current diff, registers, and ram
    save_diff(diff, DIFF_DATE_TIME_FILE);
    save_state(registers, DS1307_STATE_FILE);
    return true;
}

/* convert time and date information saved in registers into struct tm */
struct tm DS1307::get_date_time() {
    struct tm current;
    current.tm_isdst = 0;
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

// save current date registers + RAM
bool DS1307::save_state(uint8_t* state, const char* filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(state), DS1307_SIZE_REG_RAM*sizeof(uint8_t));
    return true;
}

// load date registers + RAM from file
bool DS1307::load_state(uint8_t* state, const char* filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return false;
    }

    inFile.read(reinterpret_cast<char*>(state), DS1307_SIZE_REG_RAM*sizeof(uint8_t));
    return true;
}

/* update time and date saved in registers according to set time diff and 12 or 24h mode */
void DS1307::update_date_time(long long diff, uint8_t mode_12h, uint8_t CH_bit) {
    //struct tm current = get_date_time();
    std::time_t current = DS1307::convert_tm_to_seconds(get_local_date_time());
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

void DS1307::reset_rtc() {
    long long diff = 0;
    save_diff(diff, DIFF_DATE_TIME_FILE); // initialize to 0 difference
    for (int i = 0; i < 64; i++) {
        registers[i] = 0;
    }
    save_state(registers, DS1307_STATE_FILE);
}


