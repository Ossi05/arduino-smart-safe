#pragma once
#include <Arduino.h>
#include <nrf.h>
#include "GpioPin.h"

class Keypad
{
private:
    static constexpr uint8_t ROW_COUNT = 4;
    static constexpr uint8_t COLUMN_COUNT = 4;
    static constexpr uint8_t DEBOUNCE_SAMPLES = 4;
    static constexpr uint8_t QUEUE_CAPACITY = 16;
    static constexpr uint8_t ROW_SETTLE_NOP_COUNT = 64;

    static constexpr uint32_t SCAN_TIMER_PRESCALER = 4;          // 16 MHz / 2^4 = 1 MHz tick
    static constexpr uint32_t SCAN_INTERVAL_MICROSECONDS = 5000; // 5 ms between scans
    static constexpr uint32_t SCAN_INTERRUPT_PRIORITY = 6;

    static Keypad *instance;

    GpioPin rowPins[ROW_COUNT];
    GpioPin columnPins[COLUMN_COUNT];
    char keyLayoutGrid[ROW_COUNT][COLUMN_COUNT];

    char previousScannedKey;
    char previouslyActiveKey;
    uint8_t stableReadingCount;

    volatile char keyPressQueue[QUEUE_CAPACITY];
    volatile uint8_t queueWriteIndex;
    volatile uint8_t queueReadIndex;

    void configureMatrixPins();
    void startScanTimer();
    void waitForRowToSettle() const;
    char getCurrentlyPressedKey();
    void enqueueKey(char key);

public:
    Keypad();

    bool begin();
    bool hasQueuedKey() const;
    char getNextQueuedKey();

    static void handleTimerInterrupt();
};
