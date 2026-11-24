#pragma once
#include <Arduino.h>
#include <nrf.h>
#include "GpioPin.h"
#include "NanoPinMap.h"

class ServoLock
{
private:
    static constexpr uint16_t PWM_PERIOD_MICROSECONDS = 20000; // 50 Hz servo frame at a 1 MHz PWM clock
    static constexpr uint16_t MIN_PULSE_WIDTH_MICROSECONDS = 1000;
    static constexpr uint16_t MAX_PULSE_WIDTH_MICROSECONDS = 2000;
    static constexpr uint16_t LOCKED_PULSE_WIDTH_MICROSECONDS = 1000;   // 0 degrees
    static constexpr uint16_t UNLOCKED_PULSE_WIDTH_MICROSECONDS = 1500; // 90 degrees
    static constexpr uint16_t PWM_ACTIVE_HIGH_POLARITY_FLAG = 0x8000;
    static constexpr uint8_t PWM_CHANNEL_COUNT = 4;

    const GpioPin servoPin = getNanoGpioPin(NanoPin::DIGITAL_3);
    volatile uint16_t pwmSequenceBuffer = PWM_ACTIVE_HIGH_POLARITY_FLAG | LOCKED_PULSE_WIDTH_MICROSECONDS;
    bool lockedState = true;

    void setPulseWidth(uint16_t pulseWidthMicroseconds);

public:
    void begin();
    void lock();
    void unlock();
    bool isLocked() const;
};
