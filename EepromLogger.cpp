#include "EepromLogger.h"
#include "EepromStorage.h"

String LogEvent::toString() const {
    String eventName;

    switch (event) {
        case LogEventType::SYSTEM_BOOT:     eventName = "System Boot";     break;
        case LogEventType::DOOR_UNLOCKED:   eventName = "Door Unlocked";   break;
        case LogEventType::DOOR_LOCKED:     eventName = "Door Locked";     break;
        case LogEventType::WRONG_PIN:       eventName = "Wrong PIN";       break;
        case LogEventType::ALARM_TRIGGERED: eventName = "ALARM TRIGGERED"; break;
        case LogEventType::PIN_CHANGED:     eventName = "PIN Changed";     break;
        default:                            eventName = "Unknown Event";   break;
    }

    return "[" + String(timestampSec) + "s] " + eventName;
}

void EepromLogger::begin() {
    // Check if EEPROM has already been initialized by this program
    if (readByteFromEeprom(MARKER_ADDRESS) != INITIALIZED_MARKER) {
        nextWriteSlot = 0;
        storedLogCount = 0;
        writeByteToEeprom(MARKER_ADDRESS, INITIALIZED_MARKER);
        writeByteToEeprom(NEXT_SLOT_ADDRESS, 0);
        writeByteToEeprom(NEXT_SLOT_ADDRESS + 1, 0);
        writeByteToEeprom(LOG_COUNT_ADDRESS, 0);
        writeByteToEeprom(LOG_COUNT_ADDRESS + 1, 0);
    } else {
        // Restore state after power loss
        nextWriteSlot  = (static_cast<uint16_t>(readByteFromEeprom(NEXT_SLOT_ADDRESS)) << 8)
                       | readByteFromEeprom(NEXT_SLOT_ADDRESS + 1);
        storedLogCount = (static_cast<uint16_t>(readByteFromEeprom(LOG_COUNT_ADDRESS)) << 8)
                       | readByteFromEeprom(LOG_COUNT_ADDRESS + 1);

        // Reset if EEPROM contains corrupted values
        if (nextWriteSlot >= MAX_LOGS_AMT || storedLogCount > MAX_LOGS_AMT) {
            clearLogs();
        }
    }
}

void EepromLogger::logEvent(LogEventType type) {
    LogEvent newEvent;
    newEvent.event = type;
    newEvent.timestampSec = millis() / 1000UL;

    uint16_t eepromAddress = LOGS_START_ADDRESS + (nextWriteSlot * sizeof(LogEvent));

    // Write the new event to EEPROM
    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(&newEvent);
    for (uint8_t i = 0; i < sizeof(LogEvent); i++) {
        writeByteToEeprom(eepromAddress + i, bytePtr[i]);
    }

    // Update the next write slot and stored log count
    nextWriteSlot = (nextWriteSlot + 1) % MAX_LOGS_AMT;
    if (storedLogCount < MAX_LOGS_AMT) {
        storedLogCount++;
    }

    // Save updated pointers to EEPROM
    writeByteToEeprom(NEXT_SLOT_ADDRESS, static_cast<uint8_t>(nextWriteSlot >> 8));
    writeByteToEeprom(NEXT_SLOT_ADDRESS + 1, static_cast<uint8_t>(nextWriteSlot & 0xFF));
    writeByteToEeprom(LOG_COUNT_ADDRESS, static_cast<uint8_t>(storedLogCount >> 8));
    writeByteToEeprom(LOG_COUNT_ADDRESS + 1, static_cast<uint8_t>(storedLogCount & 0xFF));
}

uint16_t EepromLogger::getAllEvents(LogEvent* eventsArray, uint16_t maxArraySize) const {
    if (eventsArray == nullptr || maxArraySize == 0) {
        return 0;
    }

    uint16_t countToRead = (storedLogCount < maxArraySize) ? storedLogCount : maxArraySize;
    uint16_t oldestSlot = (storedLogCount < MAX_LOGS_AMT) ? 0 : nextWriteSlot;

    for (uint16_t i = 0; i < countToRead; i++) {
        uint16_t currentSlot = (oldestSlot + i) % MAX_LOGS_AMT;
        uint16_t eepromAddress = LOGS_START_ADDRESS + (currentSlot * sizeof(LogEvent));

        uint8_t* bytePtr = reinterpret_cast<uint8_t*>(&eventsArray[i]);
        for (uint8_t b = 0; b < sizeof(LogEvent); b++) {
            bytePtr[b] = readByteFromEeprom(eepromAddress + b);
        }
    }

    return countToRead;
}

void EepromLogger::clearLogs() {
    nextWriteSlot = 0;
    storedLogCount = 0;
    writeByteToEeprom(NEXT_SLOT_ADDRESS, 0);
    writeByteToEeprom(NEXT_SLOT_ADDRESS + 1, 0);
    writeByteToEeprom(LOG_COUNT_ADDRESS, 0);
    writeByteToEeprom(LOG_COUNT_ADDRESS + 1, 0);
}

void EepromLogger::writeByteToEeprom(uint16_t address, uint8_t value) {
    EepromStorage::writeByte(address, value);
}

uint8_t EepromLogger::readByteFromEeprom(uint16_t address) const {
    return EepromStorage::readByte(address);
}