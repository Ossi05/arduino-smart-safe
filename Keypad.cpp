#include "Keypad.h"

Keypad* Keypad::instance = nullptr;

static constexpr char DEFAULT_KEY_LAYOUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

Keypad::Keypad(const byte rows[ROW_COUNT], const byte columns[COLUMN_COUNT])
    : previousScannedKey('\0'),
      previouslyActiveKey('\0'),
      stableReadingCount(0),
      queueWriteIndex(0),
      queueReadIndex(0) {
    
    // 1. Initialize row pin array
    for (byte row = 0; row < ROW_COUNT; row++) {
        rowPins[row] = rows[row];
    }

    // 2. Initialize column pin array
    for (byte column = 0; column < COLUMN_COUNT; column++) {
        columnPins[column] = columns[column];
    }

    // 3. Create the key layout grid
    for (byte row = 0; row < ROW_COUNT; row++) {
        for (byte column = 0; column < COLUMN_COUNT; column++) {
            keyLayoutGrid[row][column] = DEFAULT_KEY_LAYOUT[row][column];
        }
    }
}

bool Keypad::begin() {
    // 1. Setup the singleton instance
    if (instance != nullptr && instance != this) {
        Serial.println(F("Error: Only one Keypad instance is supported."));
        return false;
    }
    instance = this;

    // 2. Set row pins as OUTPUT and initialize them HIGH
    for (byte row = 0; row < ROW_COUNT; row++) {
        pinMode(rowPins[row], OUTPUT);
        digitalWrite(rowPins[row], HIGH);
    }
    
    // 3. Set column pins as INPUT_PULLUP
    for (byte column = 0; column < COLUMN_COUNT; column++) {
        pinMode(columnPins[column], INPUT_PULLUP);
    }

    // 4. Setup Timer1 for checking keypresses
    setupTimer1();
    return true;
}

void Keypad::setupTimer1() {
    noInterrupts();

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    // 5ms interval
    OCR1A = 1249;
    TCCR1B |= (1 << WGM12);              // CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10); // Prescaler 64
    TIMSK1 |= (1 << OCIE1A);             // Enable Compare Match A interrupt

    interrupts();
}

char Keypad::getCurrentlyPressedKey() {
     // 1. Loop through rows
    for (byte row = 0; row < ROW_COUNT; row++) {
        digitalWrite(rowPins[row], LOW); // Activate the current row

        // 2. Check each column for a LOW signal indicating a key press
        for (byte column = 0; column < COLUMN_COUNT; column++) {

            bool isKeyPressed = digitalRead(columnPins[column]) == LOW;
            if (isKeyPressed) {
                digitalWrite(rowPins[row], HIGH); // Reset the row back to HIGH
                char pressedKey = keyLayoutGrid[row][column];
                return pressedKey;
            }
        }

        digitalWrite(rowPins[row], HIGH); // No key press was found, reset the row back to HIGH
    }

    // No key is pressed
    return '\0';
}

void Keypad::enqueueKey(char key) {
    byte nextWriteIndex = (queueWriteIndex + 1) % QUEUE_CAPACITY; 

    if (nextWriteIndex != queueReadIndex) {
        keyPressQueueArray[queueWriteIndex] = key;
        queueWriteIndex = nextWriteIndex;
    }
}

void Keypad::handleTimerInterrupt() {
    // Check if a keypad instance exists
    if (instance == nullptr) {
        return;
    }

    // 2. Get currently pressed key
    char pressedKey = instance->getCurrentlyPressedKey();

    // 3. Check if the currently pressed key is the same as the previously scanned key
    if (pressedKey == instance->previousScannedKey) {
        if (instance->stableReadingCount < DEBOUNCE_SAMPLES) { // If the stable reading count is less than the required debounce samples
            instance->stableReadingCount++;
        } 
        else if (pressedKey != instance->previouslyActiveKey) { // If the key is different than the previously active key
            instance->previouslyActiveKey = pressedKey;

            if (pressedKey != '\0') { // If the pressed key is not the null character, enqueue it
                instance->enqueueKey(pressedKey);
            }
        }
    } else {
        instance->previousScannedKey = pressedKey;
        instance->stableReadingCount = 0;
    }
}

bool Keypad::hasQueuedKey() const {
    return queueReadIndex != queueWriteIndex;
}

char Keypad::getNextQueuedKey() {
    noInterrupts();
    if (queueReadIndex == queueWriteIndex) {
        interrupts();
        return '\0';
    }

    char nextKey = keyPressQueueArray[queueReadIndex];
    queueReadIndex = (queueReadIndex + 1) % QUEUE_CAPACITY;
    interrupts();

    return nextKey;
}

ISR(TIMER1_COMPA_vect) {
    Keypad::handleTimerInterrupt();
}