#pragma once
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

class WatchdogManager
{
private:
    bool watchdogResetOccurred;
    bool resetReasonChecked;

public:
    WatchdogManager();

    void checkAndClearResetReason();
    void begin();
    void resetWatchdog();
    void feedWatchdog();
    bool wasResetByWatchdog() const;
};
