// =============================================================================
// Hardware wiring:
//   Keypad rows      D4, D5, D6, D7   (PD4, PD5, PD6, PD7)
//   Keypad columns   D8, D9, D10, D11 (PB0, PB1, PB2, PB3)
//   ServoLock        D3               (PD3, Timer 2 PWM)
//   Screen (LCD)     SDA/SCL          (PC4/PC5 or A4/A5)
//   Buzzer           D2               (PD2, active buzzer)
//   Red LED          D12              (PB4, direct AVR DDRB/PORTB)
//   Green LED        D13              (PB5, direct AVR DDRB/PORTB)
// =============================================================================

#include <Wire.h>
#include <avr/io.h>
#include "Buzzer.h"
#include "EepromLogger.h"
#include "Keypad.h"
#include "Led.h"
#include "PasswordManager.h"
#include "SafeController.h"
#include "Screen.h"
#include "ServoLock.h"
#include "UartComms.h"

static const byte KEYPAD_ROW_PINS[4] = {4, 5, 6, 7};
static const byte KEYPAD_COLUMN_PINS[4] = {8, 9, 10, 11};

EepromLogger logger;
PasswordManager passwordManager;
Keypad keypad(KEYPAD_ROW_PINS, KEYPAD_COLUMN_PINS);
Screen screen;
ServoLock servoLock;
Buzzer buzzer;

// Red LED on D12 (PB4), Green LED on D13 (PB5) using direct AVR port registers
Led redLed(DDRB, PORTB, (1 << PB4));
Led greenLed(DDRB, PORTB, (1 << PB5));

SafeController safeController(logger, passwordManager, keypad, screen, servoLock, buzzer, redLed, greenLed);
UartComms uartComms(safeController, logger, passwordManager);

void setup()
{
    // Initialise storage layers first
    logger.begin();
    passwordManager.begin();

    // Initialise display
    screen.begin(Wire);

    // Initialise input (sets up Timer 1 ISR)
    keypad.begin();

    // Initialise servo (sets up Timer 2 PWM on D3)
    servoLock.begin();

    // Initialise safe controller (initialises LEDs, buzzer, lock, and default locked state)
    safeController.begin();

    // Initialise UART
    uartComms.begin();
}

void loop()
{
    safeController.update();
    uartComms.update();
}
