#pragma once
#include <Arduino.h>
#include <nrf.h>
#include "GpioPin.h"

class Led
{
private:
    enum class FlashMode : uint8_t
    {
        NONE,
        TIMED_THEN_PREVIOUS_STATE,
        TIMED_THEN_ON,
        CONTINUOUS,
    };

    GpioPin ledPin;

    FlashMode activeFlashMode;
    bool isLedCurrentlyOn;
    bool previousLedState;
    uint8_t remainingTransitions;
    uint16_t flashIntervalMs;
    unsigned long lastTransitionTimeMs;

    void setPinState(bool turnOn);
    void startTimedFlash(uint8_t flashCount, uint16_t intervalMs, FlashMode mode);

public:
    explicit Led(GpioPin ledPin);

    void begin();
    void on();
    void off();

    void flashAndStayOn(uint8_t flashCount, uint16_t intervalMs);
    void flash(uint8_t flashCount, uint16_t intervalMs);
    void startContinuousFlash(uint16_t intervalMs);

    void update();
};
