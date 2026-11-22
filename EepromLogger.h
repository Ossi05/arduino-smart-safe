#pragma once
#include <Arduino.h>
#include <stdint.h>

enum class LogEventType : uint8_t
{
    SYSTEM_BOOT = 0x01,
    DOOR_UNLOCKED = 0x02,
    DOOR_LOCKED = 0x03,
    WRONG_PIN = 0x04,
    ALARM_TRIGGERED = 0x05,
    PIN_CHANGED = 0x06,
    WATCHDOG_RESET = 0x07,
};

struct LogEvent
{
    LogEventType event;    // 1 byte
    uint32_t timestampSec; // 4 bytes

    const __FlashStringHelper *getEventName() const;
    String toString() const;
};

class EepromLogger
{
private:
    void writeByteToEeprom(uint16_t address, uint8_t value);
    uint8_t readByteFromEeprom(uint16_t address) const;

    static constexpr uint8_t INITIALIZED_MARKER = 0xA5;
    static constexpr uint16_t MARKER_ADDRESS = 0;      // 1 byte
    static constexpr uint16_t NEXT_SLOT_ADDRESS = 1;   // 2 bytes
    static constexpr uint16_t LOG_COUNT_ADDRESS = 3;   // 2 bytes
    static constexpr uint16_t LOGS_START_ADDRESS = 16; // Bytes 5..15 left free for PIN storage

    uint16_t nextWriteSlot = 0;
    uint16_t storedLogCount = 0;

public:
    static constexpr uint16_t MAX_LOGS_AMT = 20;

    void begin();
    void logEvent(LogEventType type);
    uint16_t getAllEvents(LogEvent *eventsArray, uint16_t maxArraySize) const;
    void clearLogs();
};