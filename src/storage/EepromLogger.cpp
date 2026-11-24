#include "EepromLogger.h"
#include "EepromStorage.h"

const __FlashStringHelper *LogEvent::getEventName() const
{
    switch (event)
    {
    case LogEventType::SYSTEM_BOOT:
        return F("System Boot");
    case LogEventType::DOOR_UNLOCKED:
        return F("Door Unlocked");
    case LogEventType::DOOR_LOCKED:
        return F("Door Locked");
    case LogEventType::WRONG_PIN:
        return F("Wrong PIN");
    case LogEventType::ALARM_TRIGGERED:
        return F("ALARM TRIGGERED");
    case LogEventType::PIN_CHANGED:
        return F("PIN Changed");
    case LogEventType::WATCHDOG_RESET:
        return F("Watchdog Reset");
    case LogEventType::WATCHDOG_INTERRUPT_RESET:
        return F("Watchdog ISR Reset");
    default:
        return F("Unknown Event");
    }
}

String LogEvent::toString() const
{
    return "[" + String(timestampSec) + "s] " + String(getEventName());
}

void EepromLogger::begin()
{
    // 1. Check if Flash storage has already been initialised
    if (readByteFromEeprom(MARKER_ADDRESS) != INITIALIZED_MARKER)
    {
        nextWriteSlot = 0;
        storedLogCount = 0;
        const uint8_t initialHeader[5] = {INITIALIZED_MARKER, 0, 0, 0, 0};
        EepromStorage::writeBytes(MARKER_ADDRESS, initialHeader, sizeof(initialHeader));
    }
    else
    {
        // 2. Restore state from Flash storage
        nextWriteSlot = (static_cast<uint16_t>(readByteFromEeprom(NEXT_SLOT_ADDRESS)) << 8) |
                        readByteFromEeprom(NEXT_SLOT_ADDRESS + 1);
        storedLogCount = (static_cast<uint16_t>(readByteFromEeprom(LOG_COUNT_ADDRESS)) << 8) |
                         readByteFromEeprom(LOG_COUNT_ADDRESS + 1);

        // 3. Reset if stored slot values are corrupted
        if (nextWriteSlot >= MAX_LOGS_AMT || storedLogCount > MAX_LOGS_AMT)
        {
            clearLogs();
        }
    }
}

void EepromLogger::logEvent(LogEventType type)
{
    LogEvent newEvent;
    newEvent.event = type;
    newEvent.timestampSec = millis() / 1000UL;

    uint16_t storageAddress = LOGS_START_ADDRESS + (nextWriteSlot * sizeof(LogEvent));

    // 1. Write the new event block to Flash storage
    const uint8_t *bytePointer = reinterpret_cast<const uint8_t *>(&newEvent);
    EepromStorage::writeBytes(storageAddress, bytePointer, sizeof(LogEvent));

    // 2. Update the next write slot and stored log count
    nextWriteSlot = (nextWriteSlot + 1) % MAX_LOGS_AMT;
    if (storedLogCount < MAX_LOGS_AMT)
    {
        storedLogCount++;
    }

    // 3. Save updated pointers to Flash storage
    const uint8_t updatedPointers[4] = {
        static_cast<uint8_t>(nextWriteSlot >> 8),
        static_cast<uint8_t>(nextWriteSlot & 0xFF),
        static_cast<uint8_t>(storedLogCount >> 8),
        static_cast<uint8_t>(storedLogCount & 0xFF)};
    EepromStorage::writeBytes(NEXT_SLOT_ADDRESS, updatedPointers, sizeof(updatedPointers));
}

uint16_t EepromLogger::getAllEvents(LogEvent *eventsArray, uint16_t maxArraySize) const
{
    if (eventsArray == nullptr || maxArraySize == 0)
    {
        return 0;
    }

    uint16_t countToRead = (storedLogCount < maxArraySize) ? storedLogCount : maxArraySize;
    uint16_t oldestSlot = (storedLogCount < MAX_LOGS_AMT) ? 0 : nextWriteSlot;

    for (uint16_t i = 0; i < countToRead; i++)
    {
        uint16_t currentSlot = (oldestSlot + i) % MAX_LOGS_AMT;
        uint16_t eepromAddress = LOGS_START_ADDRESS + (currentSlot * sizeof(LogEvent));

        uint8_t *bytePointer = reinterpret_cast<uint8_t *>(&eventsArray[i]);
        for (uint8_t b = 0; b < sizeof(LogEvent); b++)
        {
            bytePointer[b] = readByteFromEeprom(eepromAddress + b);
        }
    }

    return countToRead;
}

void EepromLogger::clearLogs()
{
    nextWriteSlot = 0;
    storedLogCount = 0;
    const uint8_t clearedPointers[4] = {0, 0, 0, 0};
    EepromStorage::writeBytes(NEXT_SLOT_ADDRESS, clearedPointers, sizeof(clearedPointers));
}

void EepromLogger::writeByteToEeprom(uint16_t address, uint8_t value)
{
    EepromStorage::writeByte(address, value);
}

uint8_t EepromLogger::readByteFromEeprom(uint16_t address) const
{
    return EepromStorage::readByte(address);
}
