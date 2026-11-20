#pragma once
#include <Arduino.h>

#include "Buzzer.h"
#include "EepromLogger.h"
#include "PasswordManager.h"
#include "Keypad.h"
#include "Screen.h"
#include "ServoLock.h"

//  0-9 ->  PIN digits
//   * -> delete last digit (backspace)
//   # -> confirm PIN / lock

enum class SafeState : uint8_t
{
    LOCKED,
    UNLOCKED,
    ALARM,
};

enum class PinEntryState : uint8_t
{
    IDLE,     // Waiting for the first keypress
    ENTERING, // User is typing digits
};

class SafeController
{
private:
    static constexpr uint8_t PIN_LENGTH = 4;
    static constexpr uint8_t MAX_PIN_LENGTH = 4;
    static constexpr uint8_t MAX_FAILED_ATTEMPTS = 3;

    EepromLogger &logger;
    PasswordManager &passwordManager;
    Keypad &keypad;
    Screen &screen;
    ServoLock &servoLock;
    Buzzer &buzzer;

    SafeState state;
    PinEntryState pinEntryState;
    uint8_t failedAttemptCount;

    // PIN buffer. null-terminated
    char pinBuffer[MAX_PIN_LENGTH + 1];
    uint8_t pinLength;

    // Keypad
    void handleKeypadInput();
    void handleDigitKey(char key);
    void handleDeleteKey();
    void handleConfirmKey();
    void resetPinEntry();

    // Display
    void showLockedUI();
    void showUnlockedUI();
    void showWrongPinUI();
    void showEnterPinUI();
    void showAlarmUI();

    void lock();
    void unlock();

public:
    SafeController(EepromLogger &logger,
                   PasswordManager &passwordManager,
                   Keypad &keypad,
                   Screen &screen,
                   ServoLock &servoLock,
                   Buzzer &buzzer);

    void begin();
    void update();

    void remoteLock();
    void remoteUnlock();

    bool isLocked() const;
    SafeState getState() const;
};
