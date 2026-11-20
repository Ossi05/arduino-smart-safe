#include "EepromStorage.h"
#include <EEPROM.h>

void EepromStorage::writeByte(uint16_t address, uint8_t value) {
    EEPROM.update(address, value);
}

uint8_t EepromStorage::readByte(uint16_t address) {
    return EEPROM.read(address);
}