#include "Keypad.h"
#include "NanoPinMap.h"

Keypad *Keypad::instance = nullptr;

static constexpr char DEFAULT_KEY_LAYOUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

Keypad::Keypad()
    : rowPins{getNanoGpioPin(NanoPin::DIGITAL_7),
              getNanoGpioPin(NanoPin::DIGITAL_6),
              getNanoGpioPin(NanoPin::DIGITAL_5),
              getNanoGpioPin(NanoPin::DIGITAL_4)},
      columnPins{getNanoGpioPin(NanoPin::DIGITAL_11),
                 getNanoGpioPin(NanoPin::DIGITAL_10),
                 getNanoGpioPin(NanoPin::DIGITAL_9),
                 getNanoGpioPin(NanoPin::DIGITAL_8)},
      previousScannedKey('\0'),
      previouslyActiveKey('\0'),
      stableReadingCount(0),
      queueWriteIndex(0),
      queueReadIndex(0)
{
    for (uint8_t row = 0; row < ROW_COUNT; row++)
    {
        for (uint8_t column = 0; column < COLUMN_COUNT; column++)
        {
            keyLayoutGrid[row][column] = DEFAULT_KEY_LAYOUT[row][column];
        }
    }
}

bool Keypad::begin()
{
    // 1. Setup the singleton instance used by the interrupt handler
    if (instance != nullptr && instance != this)
    {
        Serial.println(F("Error: Only one Keypad instance is supported."));
        return false;
    }
    instance = this;

    // 2. Configure the matrix GPIO pins
    configureMatrixPins();

    // 3. Start the 5 ms NRF_TIMER2 scan interrupt
    startScanTimer();
    return true;
}

void Keypad::configureMatrixPins()
{
    // 1. Rows R1-R4 (P0.23, P1.14, P1.13, P1.15) are open-drain so two pressed keys can never short HIGH to LOW
    for (uint8_t rowIndex = 0; rowIndex < ROW_COUNT; rowIndex++)
    {
        rowPins[rowIndex].configureAsOpenDrainOutput();
    }

    // 2. Columns C1-C4 (P1.01, P1.02, P0.27, P0.21) are inputs with internal pull-ups
    for (uint8_t columnIndex = 0; columnIndex < COLUMN_COUNT; columnIndex++)
    {
        columnPins[columnIndex].configureAsInputWithPullUp();
    }
}

void Keypad::startScanTimer()
{
    // 1. Stop and reset NRF_TIMER2 before reconfiguring it
    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->TASKS_CLEAR = 1;

    // 2. 32-bit timer mode at 1 MHz, compare match every 5 ms with automatic counter clear
    NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER2->PRESCALER = SCAN_TIMER_PRESCALER;
    NRF_TIMER2->CC[0] = SCAN_INTERVAL_MICROSECONDS;
    NRF_TIMER2->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;

    // 3. Enable only the COMPARE[0] interrupt
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    NRF_TIMER2->INTENCLR = 0xFFFFFFFFUL;
    NRF_TIMER2->INTENSET = TIMER_INTENSET_COMPARE0_Msk;

    // 4. Mbed OS relocates the vector table to RAM, so install the handler explicitly before enabling it
    NVIC_SetVector(TIMER2_IRQn, reinterpret_cast<uint32_t>(&TIMER2_IRQHandler));
    NVIC_SetPriority(TIMER2_IRQn, SCAN_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(TIMER2_IRQn);
    NVIC_EnableIRQ(TIMER2_IRQn);

    // 5. Start counting
    NRF_TIMER2->TASKS_START = 1;
}

void Keypad::waitForRowToSettle() const
{
    for (uint8_t nopIndex = 0; nopIndex < ROW_SETTLE_NOP_COUNT; nopIndex++)
    {
        __NOP();
    }
}

char Keypad::getCurrentlyPressedKey()
{
    char pressedKey = '\0';

    for (uint8_t rowIndex = 0; rowIndex < ROW_COUNT && pressedKey == '\0'; rowIndex++)
    {
        // 1. Pull only the current row LOW (OUTCLR) and let the column lines settle
        rowPins[rowIndex].setLow();
        waitForRowToSettle();

        // 2. A pressed key connects its column to the active row, so that column reads LOW in the IN register
        for (uint8_t columnIndex = 0; columnIndex < COLUMN_COUNT; columnIndex++)
        {
            if (!columnPins[columnIndex].isHigh())
            {
                pressedKey = keyLayoutGrid[rowIndex][columnIndex];
                break;
            }
        }

        // 3. Release the row (OUTSET) before scanning the next one
        rowPins[rowIndex].setHigh();
    }

    return pressedKey;
}

void Keypad::enqueueKey(char key)
{
    // 1. Calculate the next circular buffer write index
    uint8_t nextWriteIndex = (queueWriteIndex + 1) % QUEUE_CAPACITY;

    // 2. Store key if queue is not full
    if (nextWriteIndex != queueReadIndex)
    {
        keyPressQueue[queueWriteIndex] = key;
        queueWriteIndex = nextWriteIndex;
    }
}

void Keypad::handleTimerInterrupt()
{
    // 1. Verify singleton instance exists
    if (instance == nullptr)
    {
        return;
    }

    // 2. Scan matrix for currently pressed key
    char pressedKey = instance->getCurrentlyPressedKey();

    // 3. Process debounce state machine
    if (pressedKey == instance->previousScannedKey)
    {
        if (instance->stableReadingCount < DEBOUNCE_SAMPLES)
        {
            instance->stableReadingCount++;
        }
        else if (pressedKey != instance->previouslyActiveKey)
        {
            instance->previouslyActiveKey = pressedKey;

            if (pressedKey != '\0')
            {
                instance->enqueueKey(pressedKey);
            }
        }
    }
    else
    {
        instance->previousScannedKey = pressedKey;
        instance->stableReadingCount = 0;
    }
}

bool Keypad::hasQueuedKey() const
{
    return queueReadIndex != queueWriteIndex;
}

char Keypad::getNextQueuedKey()
{
    // 1. Mask only the keypad scan interrupt while touching the shared queue
    NVIC_DisableIRQ(TIMER2_IRQn);
    if (queueReadIndex == queueWriteIndex)
    {
        NVIC_EnableIRQ(TIMER2_IRQn);
        return '\0';
    }

    // 2. Retrieve next key and advance read index atomically
    char nextKey = keyPressQueue[queueReadIndex];
    queueReadIndex = (queueReadIndex + 1) % QUEUE_CAPACITY;
    NVIC_EnableIRQ(TIMER2_IRQn);

    return nextKey;
}

extern "C" void TIMER2_IRQHandler(void)
{
    if (NRF_TIMER2->EVENTS_COMPARE[0] == 0)
    {
        return;
    }

    // 1. Clear the compare event and read it back so the IRQ is not re-triggered on exit
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    (void)NRF_TIMER2->EVENTS_COMPARE[0];

    // 2. Run the debounced matrix scan
    Keypad::handleTimerInterrupt();
}
