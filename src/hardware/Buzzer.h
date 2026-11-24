#pragma once
#include <Arduino.h>
#include <nrf.h>
#include "GpioPin.h"
#include "NanoPinMap.h"

enum class BuzzerPattern : uint8_t
{
    NONE,
    KEY_PRESS,
    UNLOCK,
    LOCK,
    INCORRECT_PIN,
    ALARM,
};

class Buzzer
{
private:
    static constexpr uint8_t MAX_STEPS = 8;

    struct BeepStep
    {
        uint16_t durationMs;
        bool buzzerOn;
    };

    const GpioPin buzzerPin = getNanoGpioPin(NanoPin::DIGITAL_2);

    BuzzerPattern activePattern;
    bool patternRepeats;

    const BeepStep *currentStepTable;
    uint8_t totalStepCount;
    uint8_t currentBeepStep;
    unsigned long stepStartTimeMs;

    static const BeepStep KEY_PRESS_STEPS[];
    static const BeepStep UNLOCK_STEPS[];
    static const BeepStep LOCK_STEPS[];
    static const BeepStep INCORRECT_PIN_STEPS[];
    static const BeepStep ALARM_STEPS[];

    static const uint8_t KEY_PRESS_STEP_COUNT;
    static const uint8_t UNLOCK_STEP_COUNT;
    static const uint8_t LOCK_STEP_COUNT;
    static const uint8_t INCORRECT_PIN_STEP_COUNT;
    static const uint8_t ALARM_STEP_COUNT;

    void startPattern(BuzzerPattern pattern,
                      const BeepStep *stepTable,
                      uint8_t stepCount,
                      bool repeats);
    void applyCurrentStep();
    void setBuzzerOn();
    void setBuzzerOff();

public:
    void begin();
    void update();

    void playKeyPress();
    void playUnlockSound();
    void playLockSound();
    void playIncorrectPinSound();
    void playAlarm();

    void stopAlarm();
    void stopSound();
};
