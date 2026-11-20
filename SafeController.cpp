#include "SafeController.h"

SafeController::SafeController(EepromLogger &logger,
                               PasswordManager &passwordManager,
                               Keypad &keypad,
                               Screen &screen,
                               ServoLock &servoLock)
    : logger(logger),
      passwordManager(passwordManager),
      keypad(keypad),
      screen(screen),
      servoLock(servoLock),
      state(SafeState::LOCKED),
      pinEntryState(PinEntryState::IDLE),
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
    state = SafeState::LOCKED;
    pinEntryState = PinEntryState::IDLE;
    showLockedUI();
    logger.logEvent(LogEventType::SYSTEM_BOOT);
}

void SafeController::update()
{
    screen.update();
    handleKeypadInput();
}

bool SafeController::isLocked() const
{
    return state == SafeState::LOCKED;
}

SafeState SafeController::getState() const
{
    return state;
}

void SafeController::lock()
{
    if (state == SafeState::LOCKED)
    {
        return;
    }

    servoLock.lock();
    state = SafeState::LOCKED;
    showLockedUI();
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
    showUnlockedUI();
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
    screen.printPinCode(String(pinBuffer), MAX_PIN_LENGTH, true);

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
    screen.printPinCode(String(pinBuffer), MAX_PIN_LENGTH, false);
}

void SafeController::handleConfirmKey()
{
    if (state == SafeState::UNLOCKED)
    {
        // while open, lock the safe.
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

    // Wrong PIN
    showWrongPinUI();
    logger.logEvent(LogEventType::WRONG_PIN);
    resetPinEntry();
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
    screen.print("Enter PIN:", 0);
    screen.setStatusIcon(0, SafeIcon::LOCKED);
    screen.setStatusText("Locked");
}

void SafeController::showUnlockedUI()
{
    screen.clear();
    screen.print("Safe Unlocked", 0);
    screen.setStatusIcon(0, SafeIcon::UNLOCKED);
    screen.setStatusText("# to lock");
}

void SafeController::showWrongPinUI()
{
    screen.clear();
    screen.print("Wrong PIN!", 0);
    screen.setStatusIcon(0, SafeIcon::ALERT);
    screen.setStatusText("Try again");
}

void SafeController::showEnterPinUI()
{
    screen.clear();
    screen.print("Enter PIN:", 0);
    screen.setStatusIcon(0, SafeIcon::LOCKED);
    screen.setStatusText("Locked");
}
