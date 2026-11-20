#include "SafeController.h"

SafeController::SafeController(EepromLogger &logger,
                               PasswordManager &passwordManager,
                               Keypad &keypad,
                               Screen &screen,
                               ServoLock &servoLock,
                               Buzzer &buzzer)
    : logger(logger),
      passwordManager(passwordManager),
      keypad(keypad),
      screen(screen),
      servoLock(servoLock),
      buzzer(buzzer),
      state(SafeState::LOCKED),
      pinEntryState(PinEntryState::IDLE),
      failedAttemptCount(0),
      pinLength(0)
{
    // Zero-initialise the PIN buffer so it is always null-terminated.
    for (uint8_t i = 0; i <= MAX_PIN_LENGTH; i++)
    {
        pinBuffer[i] = '\0';
    }
}

void SafeController::begin()
{
    servoLock.lock();
    buzzer.begin();
    state = SafeState::LOCKED;
    pinEntryState = PinEntryState::IDLE;
    failedAttemptCount = 0;
    showLockedUI();
    logger.logEvent(LogEventType::SYSTEM_BOOT);
}

void SafeController::update()
{
    screen.update();
    buzzer.update();
    handleKeypadInput();
}

bool SafeController::isLocked() const
{
    // The safe is physically locked in both LOCKED and ALARM states.
    return state != SafeState::UNLOCKED;
}

SafeState SafeController::getState() const
{
    return state;
}

void SafeController::lock()
{
    // Only transition from UNLOCKED — alarm state persists until correct PIN.
    if (state != SafeState::UNLOCKED)
    {
        return;
    }

    servoLock.lock();
    state = SafeState::LOCKED;
    failedAttemptCount = 0;
    showLockedUI();
    buzzer.playLockSound();
    logger.logEvent(LogEventType::DOOR_LOCKED);
}

void SafeController::unlock()
{
    if (state == SafeState::UNLOCKED)
    {
        return;
    }

    servoLock.unlock();
    state = SafeState::UNLOCKED;
    failedAttemptCount = 0;
    showUnlockedUI();
    buzzer.stopAlarm();
    buzzer.playUnlockSound();
    logger.logEvent(LogEventType::DOOR_UNLOCKED);
}

void SafeController::handleKeypadInput()
{
    while (keypad.hasQueuedKey())
    {
        char key = keypad.getNextQueuedKey();

        switch (key)
        {
        case '#':
            handleConfirmKey();
            break;

        case '*':
            handleDeleteKey();
            break;

        case 'A':
        case 'B':
        case 'C':
        case 'D':
            break;
        default:
            if (key >= '0' && key <= '9')
            {
                handleDigitKey(key);
            }
            break;
        }
    }
}

void SafeController::handleDigitKey(char key)
{
    // Return if already unlocked
    if (state == SafeState::UNLOCKED)
        return;

    // Return if the PIN buffer is already full
    if (pinLength >= MAX_PIN_LENGTH)
        return;

    pinBuffer[pinLength] = key;
    pinLength++;
    pinBuffer[pinLength] = '\0'; // Keep null-terminated.
    pinEntryState = PinEntryState::ENTERING;
    if (state != SafeState::ALARM)
    {
        buzzer.playKeyPress();
    }
    screen.printPinCode(pinBuffer, MAX_PIN_LENGTH, true);

    // Auto-confirm once all digits are entered.
    if (pinLength == MAX_PIN_LENGTH)
    {
        handleConfirmKey();
    }
}

void SafeController::handleDeleteKey()
{
    if (state == SafeState::UNLOCKED)
        return;

    // Block delete while alarm is active so the user must retype the full PIN.
    if (state == SafeState::ALARM)
        return;

    if (pinLength == 0)
        return;

    pinLength--;
    pinBuffer[pinLength] = '\0';

    if (pinLength == 0)
    {
        pinEntryState = PinEntryState::IDLE;
        showLockedUI();
        return;
    }

    // Redraw without revealing a new digit.
    screen.printPinCode(pinBuffer, MAX_PIN_LENGTH, false);
}

void SafeController::handleConfirmKey()
{
    if (state == SafeState::UNLOCKED)
    {
        // While open, lock the safe.
        resetPinEntry();
        lock();
        return;
    }

    // Verify the entered PIN.
    if (pinLength == MAX_PIN_LENGTH && passwordManager.isCorrectPassword(pinBuffer))
    {
        resetPinEntry();
        unlock();
        return;
    }

    // Wrong PIN — either pre-alarm or during alarm.
    failedAttemptCount++;
    logger.logEvent(LogEventType::WRONG_PIN);
    resetPinEntry();

    if (failedAttemptCount >= MAX_FAILED_ATTEMPTS)
    {
        state = SafeState::ALARM;
        buzzer.playAlarm();
        showAlarmUI();
        return;
    }

    showWrongPinUI();
    buzzer.playIncorrectPinSound();
}

void SafeController::resetPinEntry()
{
    for (uint8_t i = 0; i <= MAX_PIN_LENGTH; i++)
    {
        pinBuffer[i] = '\0';
    }
    pinLength = 0;
    pinEntryState = PinEntryState::IDLE;
}

void SafeController::showLockedUI()
{
    screen.clear();
    screen.print(F("Enter PIN:"), 0);
    screen.setStatusIcon(0, SafeIcon::LOCKED);
    screen.setStatusText(F("Locked"));
}

void SafeController::showUnlockedUI()
{
    screen.clear();
    screen.print(F("Safe Unlocked"), 0);
    screen.setStatusIcon(0, SafeIcon::UNLOCKED);
    screen.setStatusText(F("# to lock"));
}

void SafeController::showWrongPinUI()
{
    screen.clear();
    screen.print(F("Wrong PIN!"), 0);
    screen.setStatusIcon(0, SafeIcon::ALERT);
    screen.setStatusText(F("Try again"));
}

void SafeController::showAlarmUI()
{
    screen.clear();
    screen.print(F("** ALARM **"), 0);
    screen.setStatusIcon(0, SafeIcon::ALERT);
    screen.setStatusText(F("Enter PIN"));
}

void SafeController::showEnterPinUI()
{
    screen.clear();
    screen.print(F("Enter PIN:"), 0);
    screen.setStatusIcon(0, SafeIcon::LOCKED);
    screen.setStatusText(F("Locked"));
}

void SafeController::remoteLock()
{
    lock();
}

void SafeController::remoteUnlock()
{
    unlock();
}