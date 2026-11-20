// =============================================================================
// Hardware wiring:
//   Keypad rows      D4, D5, D6, D7
//   Keypad columns   D8, D9, D10, D11
//   ServoLock        D3  (Timer 2 PWM)
//   Screen (LCD)     SDA/SCL (A4/A5)
//   Buzzer           D2  (PD2, active buzzer)
// =============================================================================

#include <Wire.h>
#include "Buzzer.h"
#include "EepromLogger.h"
#include "PasswordManager.h"
#include "Keypad.h"
#include "Screen.h"
#include "ServoLock.h"
#include "SafeController.h"
#include "UartComms.h"

static const byte KEYPAD_ROW_PINS[4] = {4, 5, 6, 7};
static const byte KEYPAD_COLUMN_PINS[4] = {8, 9, 10, 11};

EepromLogger logger;
PasswordManager passwordManager;
Keypad keypad(KEYPAD_ROW_PINS, KEYPAD_COLUMN_PINS);
Screen screen;
ServoLock servoLock;
Buzzer buzzer;

SafeController safeController(logger, passwordManager, keypad, screen, servoLock, buzzer);
UartComms uartComms(safeController, logger, passwordManager);

void setup()
{
    // Initialise storage layers first
    logger.begin();
    passwordManager.begin();

    // Initialise display
    screen.begin(Wire);

    // Initialise input (sets up Timer 1 ISR).
    keypad.begin();

    // Initialise servo (sets up Timer 2 PWM on D3).
    servoLock.begin();

    // Initialise the safe controller
    safeController.begin();

    // Initialise UART
    uartComms.begin();
}

void loop()
{
    safeController.update();
    uartComms.update();
}
