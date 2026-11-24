#include "WatchdogManager.h"

extern "C" void WDT_IRQHandler(void);

volatile bool WatchdogManager::watchdogInterruptTriggered = false;

extern "C" void WDT_IRQHandler(void)
{
    if (NRF_WDT->EVENTS_TIMEOUT != 0)
    {
        NRF_WDT->EVENTS_TIMEOUT = 0;
        (void)NRF_WDT->EVENTS_TIMEOUT;
    }

    WatchdogManager::handleWatchdogInterrupt();
}

void WatchdogManager::handleWatchdogInterrupt()
{
    // 1. Set volatile flag to indicate the watchdog timeout interrupt fired
    watchdogInterruptTriggered = true;

    // 2. Store emergency pre-reset marker in GPREGRET2 (survives WDT reset, unused by Nano 33 BLE bootloader)
    NRF_POWER->GPREGRET2 = EMERGENCY_FAULT_MARKER_VALUE;
}

WatchdogManager::WatchdogManager()
    : watchdogResetOccurred(false),
      emergencyMarkerDetected(false),
      resetReasonChecked(false)
{
}

void WatchdogManager::checkAndClearResetReason()
{
    if (resetReasonChecked)
    {
        return;
    }

    // 1. Inspect NRF_POWER->RESETREAS for the Watchdog Reset bit
    if ((NRF_POWER->RESETREAS & POWER_RESETREAS_DOG_Msk) != 0)
    {
        watchdogResetOccurred = true;
    }

    // 2. Inspect NRF_POWER->GPREGRET2 for the pre-reset interrupt fault marker
    if ((NRF_POWER->GPREGRET2 & 0xFFUL) == EMERGENCY_FAULT_MARKER_VALUE)
    {
        emergencyMarkerDetected = true;
        watchdogInterruptTriggered = true;
        watchdogResetOccurred = true;
    }

    // 3. Clear the watchdog reset flag (write-1-to-clear on nRF52840) and clear the retention marker
    NRF_POWER->RESETREAS = POWER_RESETREAS_DOG_Msk;
    NRF_POWER->GPREGRET2 = EMERGENCY_MARKER_CLEARED_VALUE;

    resetReasonChecked = true;
}

void WatchdogManager::begin()
{
    // 1. Ensure reset reason has been evaluated and cleared
    if (!resetReasonChecked)
    {
        checkAndClearResetReason();
    }

    // 2. Skip reconfiguration if the watchdog is already running
    if ((NRF_WDT->RUNSTATUS & WDT_RUNSTATUS_RUNSTATUS_Msk) ==
        (WDT_RUNSTATUS_RUNSTATUS_Running << WDT_RUNSTATUS_RUNSTATUS_Pos))
    {
        return;
    }

    // 3. Configure WDT to run during CPU sleep and pause while halted by a debugger
    NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
                      (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);

    // 4. Set 2-second timeout at 32.768 kHz LFCLK and enable reload register 0
    NRF_WDT->CRV = WATCHDOG_COUNTER_RELOAD_VALUE;
    NRF_WDT->RREN = WDT_RREN_RR0_Msk;

    // 5. Enable WDT timeout interrupt for pre-reset fault marker capture
    NRF_WDT->EVENTS_TIMEOUT = 0;
    NRF_WDT->INTENSET = WDT_INTENSET_TIMEOUT_Msk;
    NVIC_SetVector(WDT_IRQn, reinterpret_cast<uint32_t>(&WDT_IRQHandler));
    NVIC_SetPriority(WDT_IRQn, WATCHDOG_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(WDT_IRQn);
    NVIC_EnableIRQ(WDT_IRQn);

    // 6. Start the hardware watchdog timer
    NRF_WDT->TASKS_START = 1;
}

void WatchdogManager::resetWatchdog()
{
    NRF_WDT->RR[0] = WDT_RR_RR_VALUE;
}

bool WatchdogManager::wasResetByWatchdog() const
{
    return watchdogResetOccurred;
}

bool WatchdogManager::wasWatchdogInterruptTriggered() const
{
    return emergencyMarkerDetected || watchdogInterruptTriggered;
}
