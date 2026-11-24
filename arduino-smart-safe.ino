// =============================================================================
// Hardware wiring (Arduino Nano 33 BLE / nRF52840):
//   Keypad rows (R1-R4)      D7, D6, D5, D4   (P0.23, P1.14, P1.13, P1.15)
//   Keypad columns (C1-C4)   D11, D10, D9, D8 (P1.01, P1.02, P0.27, P0.21)
//   ServoLock                D3               (P1.12, NRF_PWM0)
//   Screen (LCD)             A4/A5            (P0.31 SDA / P0.02 SCL)
//   Buzzer                   D2               (P1.11, NRF_P1)
//   Red LED                  D12              (P1.08, NRF_P1)
//   Green LED                D13              (P0.13, NRF_P0)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <nrf.h>
#include "src/hardware/Buzzer.h"
#include "src/hardware/Keypad.h"
#include "src/hardware/Led.h"
#include "src/hardware/NanoPinMap.h"
#include "src/hardware/Screen.h"
#include "src/hardware/ServoLock.h"
#include "src/storage/EepromLogger.h"
#include "src/storage/PasswordManager.h"
#include "src/system/BleComms.h"
#include "src/system/UartComms.h"
#include "src/system/WatchdogManager.h"
#include "src/SafeController.h"

WatchdogManager watchdogManager;
EepromLogger logger;
PasswordManager passwordManager;
Keypad keypad;
Screen screen;
ServoLock servoLock;
Buzzer buzzer;

Led redLed(getNanoGpioPin(NanoPin::DIGITAL_12));
Led greenLed(getNanoGpioPin(NanoPin::DIGITAL_13));

SafeController safeController(watchdogManager, logger, passwordManager, keypad, screen, servoLock, buzzer, redLed, greenLed);
UartComms uartComms(safeController, logger, passwordManager);
BleComms bleComms(safeController, logger, passwordManager, &screen);

void setup()
{
    // 1. Evaluate reset reason and clear NRF_POWER->RESETREAS
    watchdogManager.checkAndClearResetReason();

    // 2. Initialise non-volatile Flash storage layers
    logger.begin();
    passwordManager.begin();

    // 3. Initialise display
    screen.begin(Wire);

    // 4. Initialise keypad matrix and NRF_TIMER3 5 ms scan interrupt
    keypad.begin();

    // 5. Initialise servo lock using NRF_PWM0 on D3
    servoLock.begin();

    // 6. Initialise safe controller (handles watchdog logging and default locked state)
    safeController.attachBleComms(bleComms);
    safeController.begin();

    // 7. Initialise serial communication
    uartComms.begin();

    // 8. Start BLE (advertising enabled if trusted devices exist; otherwise waiting for 'B')
    bleComms.begin();

    // 9. Configure and arm NRF_WDT with a 2-second timeout
    watchdogManager.begin();
}

void loop()
{
    watchdogManager.resetWatchdog();
    safeController.update();
    uartComms.update();
    bleComms.update();
}
