#include "Led.h"

Led::Led(GpioPin ledPin)
    : ledPin(ledPin),
      activeFlashMode(FlashMode::NONE),
      isLedCurrentlyOn(false),
      previousLedState(false),
      remainingTransitions(0),
      flashIntervalMs(0),
      lastTransitionTimeMs(0)
{
}

void Led::begin()
{
    ledPin.configureAsPushPullOutput();
    off();
}

void Led::setPinState(bool turnOn)
{
    isLedCurrentlyOn = turnOn;
    if (turnOn)
    {
        ledPin.setHigh();
    }
    else
    {
        ledPin.setLow();
    }
}

void Led::startTimedFlash(uint8_t flashCount, uint16_t intervalMs, FlashMode mode)
{
    if (flashCount == 0)
    {
        return;
    }

    previousLedState = isLedCurrentlyOn;
    activeFlashMode = mode;
    flashIntervalMs = intervalMs;
    lastTransitionTimeMs = millis();

    if (mode == FlashMode::TIMED_THEN_ON)
    {
        setPinState(true);
        remainingTransitions = (flashCount * 2) - 1;
    }
    else
    {
        setPinState(!previousLedState);
        remainingTransitions = (flashCount * 2) - 1;
    }
}

void Led::on()
{
    activeFlashMode = FlashMode::NONE;
    remainingTransitions = 0;
    setPinState(true);
}

void Led::off()
{
    activeFlashMode = FlashMode::NONE;
    remainingTransitions = 0;
    setPinState(false);
}

void Led::flashAndStayOn(uint8_t flashCount, uint16_t intervalMs)
{
    startTimedFlash(flashCount, intervalMs, FlashMode::TIMED_THEN_ON);
}

void Led::flash(uint8_t flashCount, uint16_t intervalMs)
{
    startTimedFlash(flashCount, intervalMs, FlashMode::TIMED_THEN_PREVIOUS_STATE);
}

void Led::startContinuousFlash(uint16_t intervalMs)
{
    activeFlashMode = FlashMode::CONTINUOUS;
    flashIntervalMs = intervalMs;
    lastTransitionTimeMs = millis();

    setPinState(!isLedCurrentlyOn);
}

void Led::update()
{
    if (activeFlashMode == FlashMode::NONE)
    {
        return;
    }

    // 1. Check if the active flash interval has elapsed.
    if (millis() - lastTransitionTimeMs < flashIntervalMs)
    {
        return;
    }

    lastTransitionTimeMs = millis();

    // 2. Handle continuous flashing mode.
    if (activeFlashMode == FlashMode::CONTINUOUS)
    {
        setPinState(!isLedCurrentlyOn);
        return;
    }

    // 3. Handle timed flashing transitions.
    setPinState(!isLedCurrentlyOn);
    remainingTransitions--;

    // 4. Conclude the sequence when all transitions have finished.
    if (remainingTransitions == 0)
    {
        if (activeFlashMode == FlashMode::TIMED_THEN_ON)
        {
            setPinState(true);
        }
        else
        {
            setPinState(previousLedState);
        }
        activeFlashMode = FlashMode::NONE;
    }
}
