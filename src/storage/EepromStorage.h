#pragma once
#include <Arduino.h>
#include <nrf.h>
#include <stdint.h>

class EepromStorage
{
private:
    static constexpr uint32_t FLASH_PAGE_SIZE = 4096UL;
    static constexpr uint16_t STORAGE_CAPACITY = 256;
    static constexpr uint16_t WORD_BUFFER_COUNT = STORAGE_CAPACITY / sizeof(uint32_t);

    static uint32_t getFlashPageAddress();
    static void waitForFlashReady();
    static void eraseFlashPage(uint32_t pageAddress);
    static void writeFlashWords(uint32_t pageAddress, const uint32_t *wordBuffer, uint16_t wordCount);

public:
    static void writeByte(uint16_t address, uint8_t value);
    static void writeBytes(uint16_t address, const uint8_t *data, uint16_t length);
    static uint8_t readByte(uint16_t address);
};
