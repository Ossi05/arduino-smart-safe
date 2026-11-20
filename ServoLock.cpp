#include "ServoLock.h"
#include <avr/io.h>

void ServoLock::begin() {
    // 1. Pre-load the locked pulse width and reset counter before enabling output
    OCR2B = LOCKED_PULSE;
    TCNT2 = 0;
    lockedState = true;

    // 2. Configure 8-bit Timer 2 for Fast PWM mode
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);

    // 3. Set Pin D3 as an output
    DDRD |= (1 << PD3);

    // 4. Start Timer 2 with 1024 prescaler
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
}

void ServoLock::lock() {
    setPulseWidth(LOCKED_PULSE);
    lockedState = true;
}

void ServoLock::unlock() {
    setPulseWidth(UNLOCKED_PULSE);
    lockedState = false;
}

bool ServoLock::isLocked() const {
    return lockedState;
}

void ServoLock::setPulseWidth(uint8_t ticks) {
    // Clamp value to safe servo limits so the servo cannot stall against mechanical stops
    if (ticks < MIN_PULSE_TICKS) {
        ticks = MIN_PULSE_TICKS;
    } else if (ticks > MAX_PULSE_TICKS) {
        ticks = MAX_PULSE_TICKS;
    }

    OCR2B = ticks;
}