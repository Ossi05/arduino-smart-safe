#pragma once
#include <Arduino.h>

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

    EepromLogger &logger;
    PasswordManager &passwordManager;
    Keypad &keypad;
    Screen &screen;
    ServoLock &servoLock;

    SafeState state;
    PinEntryState pinEntryState;

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

    void lock();
    void unlock();

public:
    SafeController(EepromLogger &logger,
                   PasswordManager &passwordManager,
                   Keypad &keypad,
                   Screen &screen,
                   ServoLock &servoLock);

    void begin();
    void update();

    void remoteLock();
    void remoteUnlock();

    bool isLocked() const;
    SafeState getState() const;
};
