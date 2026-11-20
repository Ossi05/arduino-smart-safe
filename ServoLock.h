#pragma once
#include <stdint.h>

/*
 * This servoLock class  uses pin D3 And Timer 2 
*/

class ServoLock {
private:
    // Timer 2 Fast PWM pulse width = (OCR2B + 1) * 64 us (at 16 MHz / 1024 prescaler)
    static constexpr uint8_t MIN_PULSE_TICKS = 8;  // ~0.58 ms hard lower limit
    static constexpr uint8_t MAX_PULSE_TICKS = 38; // ~2.50 ms hard upper limit

    static constexpr uint8_t LOCKED_PULSE   = 12;  // ~0.83 ms (0 degrees)
    static constexpr uint8_t UNLOCKED_PULSE = 23;  // ~1.54 ms (90 degrees)

    bool lockedState = true;

    void setPulseWidth(uint8_t ticks);

public:
    void begin();
    void lock();
    void unlock();
    bool isLocked() const;
};