//
// Created by ADMIN on 12/23/2023.
//

#ifndef PILL_DISPENSER_PROJECT_PILL_EEPROM_H
#define PILL_DISPENSER_PROJECT_PILL_EEPROM_H

void eepromWrite(const uint8_t *data, size_t len);
void eepromRead(const uint8_t *addr, uint8_t *dest, size_t len);
void readEELog();
int eraseEELog();
void findNewLogEntry(uint8_t *buf);
uint16_t crc16(const uint8_t *data_p, size_t length);
void writeEELog(const char *msg);

#endif //PILL_DISPENSER_PROJECT_PILL_EEPROM_H
