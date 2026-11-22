#pragma once
#include <Arduino.h>

#include "Buzzer.h"
#include "EepromLogger.h"
#include "Keypad.h"
#include "Led.h"
#include "PasswordManager.h"
#include "Screen.h"
#include "ServoLock.h"
#include "StatusDisplay.h"
#include "WatchdogManager.h"

//  0-9 -> PIN digits
//   * -> delete last digit (backspace); cancels password change when buffer is empty
//   # -> confirm PIN / lock
//   A -> enter Status Display (cycles pages on each press); D exits
//   C -> enter password-change mode (only when unlocked)
//   D -> cancel current PIN entry and return to the previous screen

enum class SafeState : uint8_t
{
    LOCKED,
    UNLOCKED,
    ALARM,
    CHANGING_PASSWORD,
    STATUS_DISPLAY,
};

class SafeController
{
private:
    static constexpr uint8_t        MAX_PIN_LENGTH                  = 4;
    static constexpr uint8_t        MAX_FAILED_ATTEMPTS             = 3;
    static constexpr unsigned long  PIN_ENTRY_TIMEOUT_MS            = 5000;
    static constexpr unsigned long  WRONG_PIN_DISPLAY_DURATION_MS   = 3000;

    WatchdogManager &watchdogManager;
    EepromLogger    &logger;
    PasswordManager &passwordManager;
    Keypad          &keypad;
    Screen          &screen;
    ServoLock       &servoLock;
    Buzzer          &buzzer;
    Led             &redLed;
    Led             &greenLed;

    SafeState     state;
    SafeState     stateBeforeStatusDisplay;
    uint8_t       failedAttemptCount;
    StatusDisplay statusDisplay;

    char          enteredPinBuffer[MAX_PIN_LENGTH + 1];
    uint8_t       enteredPinLength;
    unsigned long lastDigitInputTimeMs;
    unsigned long wrongPinDisplayStartTimeMs;

    // Keypad routing
    void handleKeypadInput();
    void handleDigitKey(char digit);
    void handleDeleteKey();
    void handleConfirmKey();
    void handleChangePasswordKey();
    void handleCancelKey();
    void handleStatusDisplayKey();

    // PIN buffer helpers
    void resetPinBuffer();
    void refreshPinDisplay(bool revealLastDigit);
    void cancelPinEntry();
    void checkPinEntryTimeout();
    void checkWrongPinDisplayTimeout();

    // Status Display helpers
    void enterStatusDisplay();
    void exitStatusDisplay();
    void checkStatusDisplayTimeout();

    // State transitions
    void confirmNewPassword();
    void cancelPasswordChange();

    // Display helpers
    void showLockedUI();
    void showUnlockedUI();
    void showWrongPinUI();
    void showAlarmUI();
    void showEnterNewPinUI();
    void showPasswordChangedUI();

public:
    SafeController(WatchdogManager &watchdogManager,
                   EepromLogger    &logger,
                   PasswordManager &passwordManager,
                   Keypad          &keypad,
                   Screen          &screen,
                   ServoLock       &servoLock,
                   Buzzer          &buzzer,
                   Led             &redLed,
                   Led             &greenLed);

    void begin();
    void update();

    void lock();
    void unlock();
    bool changePassword(const char *newPin);

    void setBluetoothConnected(bool connected);

    bool      isLocked() const;
    SafeState getState() const;
};
