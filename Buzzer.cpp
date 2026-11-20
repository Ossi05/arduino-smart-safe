#include "Buzzer.h"

const Buzzer::BeepStep Buzzer::KEY_PRESS_STEPS[] = {
    {30, true},
    {20, false},
};

const Buzzer::BeepStep Buzzer::UNLOCK_STEPS[] = {
    {80, true},
    {60, false},
    {120, true},
    {60, false},
};

const Buzzer::BeepStep Buzzer::LOCK_STEPS[] = {
    {200, true},
    {50, false},
};

const Buzzer::BeepStep Buzzer::INCORRECT_PIN_STEPS[] = {
    {80, true},
    {60, false},
    {80, true},
    {60, false},
    {80, true},
    {80, false},
};

const Buzzer::BeepStep Buzzer::ALARM_STEPS[] = {
    {100, true},
    {80, false},
};

const uint8_t Buzzer::KEY_PRESS_STEP_COUNT = sizeof(KEY_PRESS_STEPS) / sizeof(BeepStep);
const uint8_t Buzzer::UNLOCK_STEP_COUNT = sizeof(UNLOCK_STEPS) / sizeof(BeepStep);
const uint8_t Buzzer::LOCK_STEP_COUNT = sizeof(LOCK_STEPS) / sizeof(BeepStep);
const uint8_t Buzzer::INCORRECT_PIN_STEP_COUNT = sizeof(INCORRECT_PIN_STEPS) / sizeof(BeepStep);
const uint8_t Buzzer::ALARM_STEP_COUNT = sizeof(ALARM_STEPS) / sizeof(BeepStep);

void Buzzer::begin()
{
    // 1. Configure D2 (PD2) as output via direct AVR register.
    BUZZER_DDR |= (1 << BUZZER_PIN);

    // 2. Start silent.
    setBuzzerOff();

    activePattern = BuzzerPattern::NONE;
    patternRepeats = false;
    currentStepTable = nullptr;
    totalStepCount = 0;
    currentBeepStep = 0;
    stepStartTimeMs = 0;
}

void Buzzer::update()
{
    if (activePattern == BuzzerPattern::NONE)
        return;

    unsigned long now = millis();

    // 1. Check whether the current step's duration has elapsed.
    if ((now - stepStartTimeMs) < currentStepTable[currentBeepStep].durationMs)
        return;

    // 2. Advance to the next step.
    currentBeepStep++;

    if (currentBeepStep >= totalStepCount)
    {
        if (patternRepeats)
        {
            // Loop back to the first step for repeating patterns (alarm).
            currentBeepStep = 0;
        }
        else
        {
            // Pattern finished. silence the buzzer and reset state.
            setBuzzerOff();
            activePattern = BuzzerPattern::NONE;
            currentBeepStep = 0;
            return;
        }
    }

    // 4. Apply the new step immediately.
    stepStartTimeMs = now;
    applyCurrentStep();
}

void Buzzer::playKeyPress()
{
    startPattern(BuzzerPattern::KEY_PRESS, KEY_PRESS_STEPS, KEY_PRESS_STEP_COUNT, false);
}

void Buzzer::playUnlockSound()
{
    startPattern(BuzzerPattern::UNLOCK, UNLOCK_STEPS, UNLOCK_STEP_COUNT, false);
}

void Buzzer::playLockSound()
{
    startPattern(BuzzerPattern::LOCK, LOCK_STEPS, LOCK_STEP_COUNT, false);
}

void Buzzer::playIncorrectPinSound()
{
    startPattern(BuzzerPattern::INCORRECT_PIN, INCORRECT_PIN_STEPS, INCORRECT_PIN_STEP_COUNT, false);
}

void Buzzer::playAlarm()
{
    startPattern(BuzzerPattern::ALARM, ALARM_STEPS, ALARM_STEP_COUNT, true);
}

void Buzzer::stopAlarm()
{
    stopSound();
}

void Buzzer::stopSound()
{
    setBuzzerOff();
    activePattern = BuzzerPattern::NONE;
    patternRepeats = false;
    currentBeepStep = 0;
    stepStartTimeMs = 0;
}

void Buzzer::startPattern(BuzzerPattern pattern,
                          const BeepStep *stepTable,
                          uint8_t stepCount,
                          bool repeats)
{
    activePattern = pattern;
    currentStepTable = stepTable;
    totalStepCount = stepCount;
    patternRepeats = repeats;
    currentBeepStep = 0;
    stepStartTimeMs = millis();
    applyCurrentStep();
}

void Buzzer::applyCurrentStep()
{
    if (currentStepTable[currentBeepStep].buzzerOn)
        setBuzzerOn();
    else
        setBuzzerOff();
}

void Buzzer::setBuzzerOn()
{
    BUZZER_PORT |= (1 << BUZZER_PIN);
}

void Buzzer::setBuzzerOff()
{
    BUZZER_PORT &= ~(1 << BUZZER_PIN);
}
