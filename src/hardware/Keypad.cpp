#include "Keypad.h"

Keypad* Keypad::instance = nullptr;

static constexpr char DEFAULT_KEY_LAYOUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

Keypad::Keypad()
    : previousScannedKey('\0'),
      previouslyActiveKey('\0'),
      stableReadingCount(0),
      queueWriteIndex(0),
      queueReadIndex(0) {
    // 1. Initialize the key layout grid
    for (uint8_t row = 0; row < ROW_COUNT; row++) {
        for (uint8_t column = 0; column < COLUMN_COUNT; column++) {
            keyLayoutGrid[row][column] = DEFAULT_KEY_LAYOUT[row][column];
        }
    }
}

Keypad::Keypad(const uint8_t rowPins[ROW_COUNT], const uint8_t columnPins[COLUMN_COUNT])
    : Keypad() {
    (void)rowPins;
    (void)columnPins;
}

bool Keypad::begin() {
    // 1. Setup the singleton instance
    if (instance != nullptr && instance != this) {
        Serial.println(F("Error: Only one Keypad instance is supported."));
        return false;
    }
    instance = this;

    // 2. Configure row pins (PD4-PD7) as outputs and initialize them to HIGH
    DDRD |= ROW_PIN_MASK;
    PORTD |= ROW_PIN_MASK;

    // 3. Configure column pins (PB0-PB3) as inputs and enable internal pull-up resistors
    DDRB &= ~COLUMN_PIN_MASK;
    PORTB |= COLUMN_PIN_MASK;

    // 4. Setup Timer 1 for non-blocking debounced matrix scanning
    setupTimer1();
    return true;
}

void Keypad::setupTimer1() {
    // 1. Temporarily disable interrupts while configuring Timer 1
    noInterrupts();

    // 2. Reset Timer 1 control and count registers
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    // 3. Configure CTC mode with a 64 prescaler for 5ms intervals (16MHz / (64 * 200Hz) - 1 = 1249)
    OCR1A = 1249;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS11) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);

    // 4. Re-enable interrupts
    interrupts();
}

char Keypad::getCurrentlyPressedKey() {
    char pressedKey = '\0';

    // 1. Scan rows sequentially
    for (uint8_t rowIndex = 0; rowIndex < ROW_COUNT; rowIndex++) {
        // Drive current row LOW while keeping inactive rows HIGH
        PORTD = (PORTD | ROW_PIN_MASK) & ~(1 << (ROW_START_BIT + rowIndex));

        // Settling delay for pull-up voltage stabilization
        __asm__ __volatile__ ("nop\n\t" "nop\n\t");

        // 2. Read column states in a single register read operation
        uint8_t columnPinStates = PINB & COLUMN_PIN_MASK;

        // 3. Check if any column is pulled LOW by a pressed key
        if (columnPinStates != COLUMN_PIN_MASK) {
            for (uint8_t columnIndex = 0; columnIndex < COLUMN_COUNT; columnIndex++) {
                if ((columnPinStates & (1 << (COLUMN_START_BIT + columnIndex))) == 0) {
                    pressedKey = keyLayoutGrid[rowIndex][columnIndex];
                    break;
                }
            }
            break;
        }
    }

    // 4. Restore all row pins to HIGH at the end of the scan cycle
    PORTD |= ROW_PIN_MASK;

    return pressedKey;
}

void Keypad::enqueueKey(char key) {
    // 1. Calculate the next circular buffer write index
    uint8_t nextWriteIndex = (queueWriteIndex + 1) % QUEUE_CAPACITY;

    // 2. Store key if queue is not full
    if (nextWriteIndex != queueReadIndex) {
        keyPressQueueArray[queueWriteIndex] = key;
        queueWriteIndex = nextWriteIndex;
    }
}

void Keypad::handleTimerInterrupt() {
    // 1. Verify singleton instance exists
    if (instance == nullptr) {
        return;
    }

    // 2. Scan matrix for currently pressed key
    char pressedKey = instance->getCurrentlyPressedKey();

    // 3. Process debounce state machine
    if (pressedKey == instance->previousScannedKey) {
        if (instance->stableReadingCount < DEBOUNCE_SAMPLES) {
            instance->stableReadingCount++;
        } else if (pressedKey != instance->previouslyActiveKey) {
            instance->previouslyActiveKey = pressedKey;

            if (pressedKey != '\0') {
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
    // 1. Guard against reading from an empty queue with interrupts disabled
    noInterrupts();
    if (queueReadIndex == queueWriteIndex) {
        interrupts();
        return '\0';
    }

    // 2. Retrieve next key and advance read index atomically
    char nextKey = keyPressQueueArray[queueReadIndex];
    queueReadIndex = (queueReadIndex + 1) % QUEUE_CAPACITY;
    interrupts();

    return nextKey;
}

ISR(TIMER1_COMPA_vect) {
    Keypad::handleTimerInterrupt();
}
