#pragma once
#include <Arduino.h>
#include <nrf.h>

class WatchdogManager
{
private:
    static constexpr uint32_t LFCLK_FREQUENCY_HZ = 32768UL;
    static constexpr uint32_t WATCHDOG_TIMEOUT_SECONDS = 2UL;
    static constexpr uint32_t WATCHDOG_COUNTER_RELOAD_VALUE = LFCLK_FREQUENCY_HZ * WATCHDOG_TIMEOUT_SECONDS;
    static constexpr uint32_t WATCHDOG_INTERRUPT_PRIORITY = 0;

    static constexpr uint8_t EMERGENCY_FAULT_MARKER_VALUE = 0xA5;
    static constexpr uint8_t EMERGENCY_MARKER_CLEARED_VALUE = 0x00;

    static volatile bool watchdogInterruptTriggered;

    bool watchdogResetOccurred;
    bool emergencyMarkerDetected;
    bool resetReasonChecked;

public:
    WatchdogManager();

    void checkAndClearResetReason();
    void begin();
    void resetWatchdog();
    bool wasResetByWatchdog() const;
    bool wasWatchdogInterruptTriggered() const;

    static void handleWatchdogInterrupt();
};
