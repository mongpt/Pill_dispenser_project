//
// Created by ADMIN on 12/23/2023.
//
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "string.h"
#include "pill_eeprom.h"
#include "lorawan.h"

#define EE_ADDR 0x50
#define WRITE_CYCLE_TIME 5

//write len data bytes to eeprom including 2 bytes word address
void eepromWrite(const uint8_t *data, size_t len){
    i2c_write_blocking(i2c0, EE_ADDR, data, len,false);
    sleep_ms(WRITE_CYCLE_TIME);
}

// read len data bytes back from eeprom at specified address
void eepromRead(const uint8_t *addr, uint8_t *dest, size_t len){
    i2c_write_blocking(i2c0, EE_ADDR, addr,2,true);
    sleep_ms(WRITE_CYCLE_TIME);
    i2c_read_blocking(i2c0,EE_ADDR, dest, len, false);
}

void readEELog(){
    printf("\n");
    // read log from eeprom
    uint8_t logData[64] = {0};
    uint8_t buf[3];
    bool foundLog = false;
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &buf[2], 1);
        if (buf[2]){ // found a valid log entry
            foundLog = true;
            eepromRead(buf, logData, 64);
            int zero_idx;
            for (zero_idx = 0; zero_idx < 62; zero_idx++){   // find index of 0 before CRC value
                if (logData[zero_idx] == 0){    //found 0 at zero_idx position
                    if (crc16(logData, zero_idx+3) == 0) {
                        printf("%s\n", logData);
                    }
                    break;
                }
            }
        }
    }
    if (!foundLog) {
        //*logData = (uint8_t) "tsgh";
        printf("No log found\n");
    }
}

int eraseEELog(){
    uint8_t buf[3];
    buf[2] = 0x00;  //data
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromWrite(buf, 3);
    }

    // validate
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &buf[2], 1);
        if (buf[2])
            return 1;
    }
    return 0;
}

// find a new valid entry for new logging
void findNewLogEntry(uint8_t *buf){
    uint8_t data;
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &data, 1);
        if (!data) // found a valid address if its data is 0
            break;
    }
    if (data){  //all log entries are not available, need to erase them
        while(eraseEELog());    // if this function return 1 (means erasing did not succeed) then do again
        buf[0] = 0x00;
        buf[1] = 0x00;
    }
}

//CRC calculation
uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t) (x << 12)) ^ ((uint16_t) (x << 5)) ^ ((uint16_t) x);
    }
    return crc;
}

//write log to eeprom
void writeEELog(const char *msg){
    printf("%s\n", msg);

    uint8_t length = strlen(msg);
    //trim log string if it exceeds 61 chars
    if (length > 61)
        length = 61;
    uint8_t bufLogging[length+3]; // 2 bytes address + log string + \0 + 2 bytes CRC
    uint8_t tempData[length+1]; //log string + \0
    // find the next log address for new log
    findNewLogEntry(bufLogging);    // get data for bufLogging[0] and bufLogging[1]
    for (int i = 0; i < length; i++)
        bufLogging[i+2] = msg[i];
    bufLogging[length+2] = '\0';
    memcpy(tempData, msg, strlen(msg));
    tempData[length] = '\0';
    // calculate CRC then store it at idx+3 and idx+4 location
    uint16_t crc = crc16(tempData, sizeof(tempData));
    bufLogging[length+3] = (uint8_t) (crc >> 8);
    bufLogging[length+4] = (uint8_t) crc;
    // write log to eeprom
    eepromWrite(bufLogging, length+5);
}

