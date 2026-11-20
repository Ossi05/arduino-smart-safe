#pragma once
#include <stdint.h>

class EepromStorage {
public:
    static void writeByte(uint16_t address, uint8_t value);
    static uint8_t readByte(uint16_t address);
};