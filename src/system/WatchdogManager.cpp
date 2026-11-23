#include "WatchdogManager.h"

volatile bool WatchdogManager::watchdogInterruptTriggered = false;

ISR(WDT_vect)
{
    WatchdogManager::handleWatchdogInterrupt();
}

void WatchdogManager::handleWatchdogInterrupt()
{
    // 1. Set volatile flag to indicate the watchdog interrupt fired
    watchdogInterruptTriggered = true;

    // 2. Wait for completion of any previous EEPROM write
    while (EECR & (1 << EEPE))
    {
    }

    // 3. Set address and data registers for emergency pre-reset fault marker
    EEAR = EMERGENCY_FAULT_MARKER_ADDRESS;
    EEDR = EMERGENCY_FAULT_MARKER_VALUE;

    // 4. Initiate direct EEPROM timed write sequence
    EECR |= (1 << EEMPE);
    EECR |= (1 << EEPE);
}

WatchdogManager::WatchdogManager()
    : watchdogResetOccurred(false),
      emergencyMarkerDetected(false),
      resetReasonChecked(false)
{
    checkAndClearResetReason();
}

void WatchdogManager::checkAndClearResetReason()
{
    if (resetReasonChecked)
    {
        return;
    }

    // 1. Inspect MCU Status Register for Watchdog Reset Flag (WDRF)
    if (MCUSR & (1 << WDRF))
    {
        watchdogResetOccurred = true;
    }

    // 2. Inspect EEPROM emergency fault marker written by ISR(WDT_vect)
    uint8_t storedMarker = readEepromByteDirect(EMERGENCY_FAULT_MARKER_ADDRESS);
    if (storedMarker == EMERGENCY_FAULT_MARKER_VALUE)
    {
        emergencyMarkerDetected = true;
        watchdogInterruptTriggered = true;
        watchdogResetOccurred = true;
    }

    // 3. Clear all reset flags in MCUSR
    MCUSR = 0;

    // 4. Clear emergency EEPROM fault marker byte
    if (storedMarker != EMERGENCY_MARKER_CLEARED_VALUE)
    {
        writeEepromByteDirect(EMERGENCY_FAULT_MARKER_ADDRESS, EMERGENCY_MARKER_CLEARED_VALUE);
    }

    // 5. Disable watchdog timer immediately to prevent bootloader reset loops
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

    // 2. Configure Watchdog Timer for Interrupt + System Reset Mode with a 2-second timeout
    uint8_t savedStatusRegister = SREG;
    cli();
    __asm__ __volatile__("wdr");
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
    SREG = savedStatusRegister;
}

void WatchdogManager::resetWatchdog()
{
    __asm__ __volatile__("wdr");
}

bool WatchdogManager::wasResetByWatchdog() const
{
    return watchdogResetOccurred;
}

bool WatchdogManager::wasWatchdogInterruptTriggered() const
{
    return emergencyMarkerDetected || watchdogInterruptTriggered;
}

void WatchdogManager::writeEepromByteDirect(uint16_t address, uint8_t value)
{
    while (EECR & (1 << EEPE))
    {
    }
    EEAR = address;
    EEDR = value;
    EECR |= (1 << EEMPE);
    EECR |= (1 << EEPE);
    while (EECR & (1 << EEPE))
    {
    }
}

uint8_t WatchdogManager::readEepromByteDirect(uint16_t address)
{
    while (EECR & (1 << EEPE))
    {
    }
    EEAR = address;
    EECR |= (1 << EERE);
    return EEDR;
}
