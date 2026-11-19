#pragma once
#include <Arduino.h>

class Keypad {
private:
    static constexpr byte ROW_COUNT = 4;
    static constexpr byte COLUMN_COUNT = 4;
    static constexpr byte DEBOUNCE_SAMPLES = 4; // 4 * 5ms = 20ms
    static constexpr byte QUEUE_CAPACITY = 16;

    static Keypad* instance;

    byte rowPins[ROW_COUNT];
    byte columnPins[COLUMN_COUNT];
    char keyLayoutGrid[ROW_COUNT][COLUMN_COUNT];

    char previousScannedKey;
    char previouslyActiveKey;
    byte stableReadingCount;

    volatile char keyPressQueueArray[QUEUE_CAPACITY];
    volatile byte queueWriteIndex;
    volatile byte queueReadIndex;

    char getCurrentlyPressedKey();
    void enqueueKey(char key);
    void setupTimer1();

public:
    Keypad(const byte rows[ROW_COUNT], const byte columns[COLUMN_COUNT]);

    bool begin();
    bool hasQueuedKey() const;
    char getNextQueuedKey();

    static void handleTimerInterrupt();
};