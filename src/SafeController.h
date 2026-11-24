#pragma once
#include <Arduino.h>

#include "hardware/Buzzer.h"
#include "storage/EepromLogger.h"
#include "hardware/Keypad.h"
#include "hardware/Led.h"
#include "storage/PasswordManager.h"
#include "hardware/Screen.h"
#include "hardware/ServoLock.h"
#include "hardware/StatusDisplay.h"
#include "system/WatchdogManager.h"

//  0-9 -> PIN digits
//   * -> delete last digit (backspace); cancels password change when buffer is empty
//   # -> confirm PIN / lock
//   A -> enter Status Display (cycles pages on each press); D exits
//   C -> enter password-change mode (only when unlocked)
//   D -> cancel current PIN entry and return to the previous screen

class BleComms;

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
    static constexpr unsigned long  BLUETOOTH_MESSAGE_DURATION_MS   = 3000;
    static constexpr uint8_t        NO_PAIRING_SECONDS_DISPLAYED    = 0xFF;

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

    BleComms     *bleComms;
    bool          isBluetoothScreenActive;
    bool          isBluetoothMessageTimed;
    unsigned long bluetoothMessageStartTimeMs;
    uint8_t       displayedPairingSeconds;

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

    // Bluetooth pairing helpers
    void handleBluetoothPairingKey();
    void updateBluetoothDisplay();
    void startTimedBluetoothMessage(const char *message);
    void showUnlockToPairUI();
    void showBluetoothPairingUI(uint8_t remainingSeconds);

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

    void attachBleComms(BleComms &bleComms);
    void setBluetoothConnected(bool connected);

    bool      isLocked() const;
    SafeState getState() const;
};
