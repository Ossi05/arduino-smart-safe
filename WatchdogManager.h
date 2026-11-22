#pragma once
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

class WatchdogManager
{
private:
    static constexpr uint16_t EMERGENCY_FAULT_MARKER_ADDRESS = 10;
    static constexpr uint8_t EMERGENCY_FAULT_MARKER_VALUE = 0xA5;
    static constexpr uint8_t EMERGENCY_MARKER_CLEARED_VALUE = 0x00;

    static volatile bool watchdogInterruptTriggered;

    bool watchdogResetOccurred;
    bool emergencyMarkerDetected;
    bool resetReasonChecked;

    static void writeEepromByteDirect(uint16_t address, uint8_t value);
    static uint8_t readEepromByteDirect(uint16_t address);

public:
    WatchdogManager();

    void checkAndClearResetReason();
    void begin();
    void resetWatchdog();
    bool wasResetByWatchdog() const;
    bool wasWatchdogInterruptTriggered() const;

    static void handleWatchdogInterrupt();
};
