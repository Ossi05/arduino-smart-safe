#pragma once
#include <Arduino.h>
#include <avr/io.h>

class Keypad
{
private:
    static constexpr uint8_t ROW_COUNT = 4;
    static constexpr uint8_t COLUMN_COUNT = 4;
    static constexpr uint8_t DEBOUNCE_SAMPLES = 4;
    static constexpr uint8_t QUEUE_CAPACITY = 16;

    static constexpr uint8_t R1_BIT = PD7;
    static constexpr uint8_t C1_BIT = PB3;
    static constexpr uint8_t ROW_PIN_MASK = (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    static constexpr uint8_t COLUMN_PIN_MASK = (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);

    static Keypad *instance;

    char keyLayoutGrid[ROW_COUNT][COLUMN_COUNT];

    char previousScannedKey;
    char previouslyActiveKey;
    uint8_t stableReadingCount;

    volatile char keyPressQueueArray[QUEUE_CAPACITY];
    volatile uint8_t queueWriteIndex;
    volatile uint8_t queueReadIndex;

    char getCurrentlyPressedKey();
    void enqueueKey(char key);
    void setupTimer1();

public:
    Keypad();
    Keypad(const uint8_t rowPins[ROW_COUNT], const uint8_t columnPins[COLUMN_COUNT]);

    bool begin();
    bool hasQueuedKey() const;
    char getNextQueuedKey();

    static void handleTimerInterrupt();
};