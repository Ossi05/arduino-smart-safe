#include "EepromStorage.h"

// Dedicated 4 KB-aligned page in Flash reserved by the linker so code is never overwritten.
alignas(4096) static const uint8_t RESERVED_FLASH_STORAGE_PAGE[4096] __attribute__((used)) = {0};

uint32_t EepromStorage::getFlashPageAddress()
{
    return reinterpret_cast<uint32_t>(RESERVED_FLASH_STORAGE_PAGE);
}

void EepromStorage::waitForFlashReady()
{
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
    }
}

void EepromStorage::eraseFlashPage(uint32_t pageAddress)
{
    // 1. Enable Flash page erase mode in NRF_NVMC
    waitForFlashReady();
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
    waitForFlashReady();

    // 2. Erase the 4 KB page
    NRF_NVMC->ERASEPAGE = pageAddress;
    waitForFlashReady();

    // 3. Return NRF_NVMC to read-only mode
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    waitForFlashReady();
}

void EepromStorage::writeFlashWords(uint32_t pageAddress, const uint32_t *wordBuffer, uint16_t wordCount)
{
    // 1. Enable Flash write mode in NRF_NVMC
    waitForFlashReady();
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    waitForFlashReady();

    // 2. Program 32-bit aligned words sequentially into Flash
    volatile uint32_t *flashWords = reinterpret_cast<volatile uint32_t *>(pageAddress);
    for (uint16_t wordIndex = 0; wordIndex < wordCount; wordIndex++)
    {
        flashWords[wordIndex] = wordBuffer[wordIndex];
        waitForFlashReady();
    }

    // 3. Return NRF_NVMC to read-only mode
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    waitForFlashReady();
}

void EepromStorage::writeByte(uint16_t address, uint8_t value)
{
    writeBytes(address, &value, 1);
}

void EepromStorage::writeBytes(uint16_t address, const uint8_t *data, uint16_t length)
{
    if (data == nullptr || length == 0 || address >= STORAGE_CAPACITY || length > (STORAGE_CAPACITY - address))
    {
        return;
    }

    uint32_t pageAddress = getFlashPageAddress();
    const volatile uint8_t *flashBytes = reinterpret_cast<const volatile uint8_t *>(pageAddress);

    // 1. Skip erase/write if Flash already contains the exact target bytes
    bool hasChanges = false;
    for (uint16_t byteIndex = 0; byteIndex < length; byteIndex++)
    {
        if (flashBytes[address + byteIndex] != data[byteIndex])
        {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges)
    {
        return;
    }

    // 2. Copy the current storage block into a 32-bit aligned RAM buffer
    uint32_t ramWordBuffer[WORD_BUFFER_COUNT];
    uint8_t *ramByteBuffer = reinterpret_cast<uint8_t *>(ramWordBuffer);
    for (uint16_t byteIndex = 0; byteIndex < STORAGE_CAPACITY; byteIndex++)
    {
        ramByteBuffer[byteIndex] = flashBytes[byteIndex];
    }

    // 3. Apply the requested byte updates in RAM
    for (uint16_t byteIndex = 0; byteIndex < length; byteIndex++)
    {
        ramByteBuffer[address + byteIndex] = data[byteIndex];
    }

    // 4. Erase the Flash page and program the updated block atomically with respect to IRQs
    uint32_t savedPrimask = __get_PRIMASK();
    __disable_irq();
    eraseFlashPage(pageAddress);
    writeFlashWords(pageAddress, ramWordBuffer, WORD_BUFFER_COUNT);
    __set_PRIMASK(savedPrimask);
}

uint8_t EepromStorage::readByte(uint16_t address)
{
    if (address >= STORAGE_CAPACITY)
    {
        return 0xFF;
    }

    const volatile uint8_t *flashBytes = reinterpret_cast<const volatile uint8_t *>(getFlashPageAddress());
    return flashBytes[address];
}
