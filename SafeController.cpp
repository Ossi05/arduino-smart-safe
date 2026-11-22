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
      stateBeforeStatusDisplay(SafeState::LOCKED),
      failedAttemptCount(0),
      statusDisplay(screen),
      enteredPinLength(0),
      lastDigitInputTimeMs(0),
      wrongPinDisplayStartTimeMs(0)
{
    // Zero-initialise the PIN buffer so it is always null-terminated.
    for (uint8_t i = 0; i <= MAX_PIN_LENGTH; i++)
    {
        enteredPinBuffer[i] = '\0';
    }
}

void SafeController::begin()
{
    servoLock.lock();
    buzzer.begin();
    state = SafeState::LOCKED;
    failedAttemptCount = 0;
    showLockedUI();
    logger.logEvent(LogEventType::SYSTEM_BOOT);
}

void SafeController::update()
{
    screen.update();
    buzzer.update();
    checkPinEntryTimeout();
    checkWrongPinDisplayTimeout();
    checkStatusDisplayTimeout();
    handleKeypadInput();
}

bool SafeController::isLocked() const
{
    return state != SafeState::UNLOCKED && state != SafeState::CHANGING_PASSWORD;
}

SafeState SafeController::getState() const
{
    return state;
}

void SafeController::setBluetoothConnected(bool connected)
{
    statusDisplay.setBluetoothConnected(connected);
}

void SafeController::lock()
{
    // Only transition from states where the servo is open.
    if (state != SafeState::UNLOCKED && state != SafeState::CHANGING_PASSWORD)
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

bool SafeController::changePassword(const char *newPin)
{
    if (!passwordManager.setPassword(newPin))
    {
        return false;
    }
    logger.logEvent(LogEventType::PIN_CHANGED);
    return true;
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
            handleStatusDisplayKey();
            break;
        case 'C':
            handleChangePasswordKey();
            break;
        case 'D':
            handleCancelKey();
            break;
        case 'B':
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

void SafeController::handleDigitKey(char digit)
{
    if (state == SafeState::UNLOCKED || state == SafeState::STATUS_DISPLAY)
        return;

    if (enteredPinLength >= MAX_PIN_LENGTH)
        return;

    // 1. Append digit to buffer.
    enteredPinBuffer[enteredPinLength] = digit;
    enteredPinLength++;
    enteredPinBuffer[enteredPinLength] = '\0';
    lastDigitInputTimeMs = millis();
    wrongPinDisplayStartTimeMs = 0; // Dismiss wrong-PIN screen as soon as typing resumes.

    // 2. Play keypress sound
    if (state != SafeState::ALARM)
    {
        buzzer.playKeyPress();
    }

    // 3. Redraw the PIN row
    refreshPinDisplay(true);

    // 4. Auto-confirm once all digits are entered
    if (enteredPinLength == MAX_PIN_LENGTH)
    {
        handleConfirmKey();
    }
}

void SafeController::handleDeleteKey()
{
    if (state == SafeState::STATUS_DISPLAY)
        return;

    if (state == SafeState::UNLOCKED)
        return;

    // During an alarm the user must retype the full PIN block backspace.
    if (state == SafeState::ALARM)
        return;

    if (enteredPinLength == 0)
    {
        // If changing the password, pressing * on an empty buffer cancels the change.
        if (state == SafeState::CHANGING_PASSWORD)
        {
            cancelPasswordChange();
        }
        return;
    }

    // 2. Remove last digit and redraw.
    enteredPinLength--;
    enteredPinBuffer[enteredPinLength] = '\0';
    lastDigitInputTimeMs = millis();

    if (enteredPinLength == 0)
    {
        // Buffer is now empty — restore the prompt for the current state.
        if (state == SafeState::CHANGING_PASSWORD)
        {
            showEnterNewPinUI();
        }
        else
        {
            showLockedUI();
        }
        return;
    }

    refreshPinDisplay(false);
}

void SafeController::handleConfirmKey()
{
    if (state == SafeState::STATUS_DISPLAY)
        return;

    // 1. While open, # locks the safe.
    if (state == SafeState::UNLOCKED)
    {
        resetPinBuffer();
        lock();
        return;
    }

    // 2. Finalise new password entry.
    if (state == SafeState::CHANGING_PASSWORD)
    {
        confirmNewPassword();
        return;
    }

    // 3. Require a full PIN before evaluating, pressing # on a partial entry
    //    is a no-op, not a failed attempt.
    if (enteredPinLength < MAX_PIN_LENGTH)
        return;

    // 4. Correct PIN -> unlock.
    if (passwordManager.isCorrectPassword(enteredPinBuffer))
    {
        resetPinBuffer();
        unlock();
        return;
    }

    // 5. Wrong PIN.
    failedAttemptCount++;
    logger.logEvent(LogEventType::WRONG_PIN);
    resetPinBuffer();

    if (failedAttemptCount >= MAX_FAILED_ATTEMPTS)
    {
        state = SafeState::ALARM;
        buzzer.playAlarm();
        showAlarmUI();
        logger.logEvent(LogEventType::ALARM_TRIGGERED);
        return;
    }

    showWrongPinUI();
    wrongPinDisplayStartTimeMs = millis();
    buzzer.playIncorrectPinSound();
}

void SafeController::handleChangePasswordKey()
{
    // If the status display is open, check whether the safe was unlocked before entering it.
    if (state == SafeState::STATUS_DISPLAY)
    {
        if (stateBeforeStatusDisplay != SafeState::UNLOCKED)
        {
            return;
        }

        // Close the status display without restoring the unlocked screen first,
        // so we can go straight to the password-change screen.
        statusDisplay.deactivate();
        state = SafeState::UNLOCKED;
    }

    if (state != SafeState::UNLOCKED)
    {
        return;
    }

    // Enter password-change mode and prompt for the new PIN.
    resetPinBuffer();
    state = SafeState::CHANGING_PASSWORD;
    showEnterNewPinUI();
}

void SafeController::handleStatusDisplayKey()
{
    if (state == SafeState::STATUS_DISPLAY)
    {
        bool lastPageReached = statusDisplay.showNextPage();
        if (lastPageReached)
        {
            exitStatusDisplay();
        }
        return;
    }

    // Status Display is not available during an active alarm.
    if (state == SafeState::ALARM)
        return;

    enterStatusDisplay();
}

void SafeController::resetPinBuffer()
{
    for (uint8_t i = 0; i <= MAX_PIN_LENGTH; i++)
    {
        enteredPinBuffer[i] = '\0';
    }
    enteredPinLength = 0;
    lastDigitInputTimeMs = 0;
}

void SafeController::refreshPinDisplay(bool revealLastDigit)
{
    screen.printPinCode(enteredPinBuffer, MAX_PIN_LENGTH, revealLastDigit);
}

void SafeController::cancelPinEntry()
{
    resetPinBuffer();

    if (state == SafeState::CHANGING_PASSWORD)
    {
        showEnterNewPinUI();
    }
    else if (state == SafeState::ALARM)
    {
        showAlarmUI();
    }
    else
    {
        showLockedUI();
    }
}

void SafeController::checkPinEntryTimeout()
{
    if (enteredPinLength == 0 || state == SafeState::UNLOCKED)
        return;

    if (state == SafeState::STATUS_DISPLAY)
        return;

    if (millis() - lastDigitInputTimeMs >= PIN_ENTRY_TIMEOUT_MS)
    {
        cancelPinEntry();
    }
}

void SafeController::checkWrongPinDisplayTimeout()
{
    if (wrongPinDisplayStartTimeMs == 0)
        return;

    if (millis() - wrongPinDisplayStartTimeMs >= WRONG_PIN_DISPLAY_DURATION_MS)
    {
        wrongPinDisplayStartTimeMs = 0;
        showLockedUI();
    }
}

void SafeController::handleCancelKey()
{
    // D always exits Status Display immediately, regardless of prior state.
    if (state == SafeState::STATUS_DISPLAY)
    {
        exitStatusDisplay();
        return;
    }

    if (state == SafeState::UNLOCKED)
        return;

    cancelPinEntry();
}

void SafeController::enterStatusDisplay()
{
    stateBeforeStatusDisplay = state;
    state = SafeState::STATUS_DISPLAY;
    resetPinBuffer();
    statusDisplay.activate();
}

void SafeController::exitStatusDisplay()
{
    statusDisplay.deactivate();
    state = stateBeforeStatusDisplay;

    // Restore the correct screen for the state we are returning to.
    switch (state)
    {
    case SafeState::UNLOCKED:
        showUnlockedUI();
        break;

    case SafeState::CHANGING_PASSWORD:
        showEnterNewPinUI();
        break;

    case SafeState::ALARM:
        showAlarmUI();
        break;

    case SafeState::LOCKED:
    default:
        showLockedUI();
        break;
    }
}

void SafeController::checkStatusDisplayTimeout()
{
    if (state != SafeState::STATUS_DISPLAY)
        return;

    bool didTimeout = statusDisplay.checkAutoExitTimeout();
    if (didTimeout)
    {
        state = stateBeforeStatusDisplay;

        switch (state)
        {
        case SafeState::UNLOCKED:
            showUnlockedUI();
            break;

        case SafeState::CHANGING_PASSWORD:
            showEnterNewPinUI();
            break;

        case SafeState::ALARM:
            showAlarmUI();
            break;

        case SafeState::LOCKED:
        default:
            showLockedUI();
            break;
        }
    }
}

void SafeController::confirmNewPassword()
{
    if (enteredPinLength != MAX_PIN_LENGTH)
    {
        return;
    }

    if (changePassword(enteredPinBuffer))
    {
        resetPinBuffer();
        state = SafeState::UNLOCKED;
        showPasswordChangedUI();
        buzzer.playUnlockSound();
    }
    else
    {
        resetPinBuffer();
        showEnterNewPinUI();
    }
}

void SafeController::cancelPasswordChange()
{
    resetPinBuffer();
    state = SafeState::UNLOCKED;
    showUnlockedUI();
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

void SafeController::showEnterNewPinUI()
{
    screen.clear();
    screen.print(F("New PIN:"), 0);
    screen.setStatusIcon(0, SafeIcon::UNLOCKED);
    screen.setStatusText(F("* cancel"));
}

void SafeController::showPasswordChangedUI()
{
    screen.clear();
    screen.print(F("PIN changed!"), 0);
    screen.setStatusIcon(0, SafeIcon::UNLOCKED);
    screen.setStatusText(F("# to lock"));
}
