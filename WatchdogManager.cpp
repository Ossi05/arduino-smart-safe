#include "WatchdogManager.h"

WatchdogManager::WatchdogManager()
    : watchdogResetOccurred(false),
      resetReasonChecked(false)
{
    // Evaluate reset reason early during object construction
    checkAndClearResetReason();
}

void WatchdogManager::checkAndClearResetReason()
{
    if (resetReasonChecked)
    {
        return;
    }

    // 1. Read MCU Status Register to inspect if the reset was triggered by the watchdog
    if (MCUSR & (1 << WDRF))
    {
        watchdogResetOccurred = true;
    }

    // 2. Clear all reset flags in MCUSR
    MCUSR = 0;

    // 3. Disable the watchdog timer immediately to prevent bootloader reset loops on reboot
    uint8_t savedStatusRegister = SREG;
    cli();
    __asm__ __volatile__("wdr");
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0x00;
    SREG = savedStatusRegister;

    resetReasonChecked = true;
}

void WatchdogManager::begin()
{
    // 1. Ensure reset reason has been checked and MCUSR cleared
    if (!resetReasonChecked)
    {
        checkAndClearResetReason();
    }

    // 2. Configure Watchdog Timer for a 2-second timeout using timed register sequence
    uint8_t savedStatusRegister = SREG;
    cli();
    __asm__ __volatile__("wdr");
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
    SREG = savedStatusRegister;
}

void WatchdogManager::resetWatchdog()
{
    __asm__ __volatile__("wdr");
}

void WatchdogManager::feedWatchdog()
{
    resetWatchdog();
}

bool WatchdogManager::wasResetByWatchdog() const
{
    return watchdogResetOccurred;
}
