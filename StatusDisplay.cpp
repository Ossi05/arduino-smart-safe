
#include "StatusDisplay.h"

StatusDisplay::StatusDisplay(Screen &screen)
    : screen(screen),
      isActive(false),
      currentPage(StatusPage::BLUETOOTH),
      lastInteractionTimeMs(0),
      isBluetoothConnected(false)
{
}

void StatusDisplay::setBluetoothConnected(bool connected)
{
    isBluetoothConnected = connected;
}

void StatusDisplay::activate()
{
    isActive = true;
    currentPage = StatusPage::BLUETOOTH;
    lastInteractionTimeMs = millis();

    renderCurrentPage();
}

bool StatusDisplay::showNextPage()
{
    lastInteractionTimeMs = millis();

    // 1. Advance to the next page index.
    uint8_t nextIndex =
        static_cast<uint8_t>(currentPage) + 1;

    bool wrappedAround = (nextIndex >= static_cast<uint8_t>(StatusPage::STATUS_PAGE_COUNT));

    // 2. If we have gone past the last page, signal the caller to exit instead of wrapping.
    if (wrappedAround)
    {
        return true;
    }

    currentPage = static_cast<StatusPage>(nextIndex);

    // 3. Redraw.
    renderCurrentPage();
    return false;
}

void StatusDisplay::deactivate()
{
    isActive = false;
}

bool StatusDisplay::checkAutoExitTimeout()
{
    if (!isActive)
        return false;

    if (millis() - lastInteractionTimeMs >= AUTO_EXIT_TIMEOUT_MS)
    {
        deactivate();
        return true;
    }

    return false;
}

bool StatusDisplay::active() const
{
    return isActive;
}

void StatusDisplay::renderCurrentPage()
{
    switch (currentPage)
    {
    case StatusPage::BLUETOOTH:
        renderBluetoothStatus();
        break;
    default:
        break;
    }
}

void StatusDisplay::renderBluetoothStatus()
{
    // 1. Page header on row 0.
    screen.print(F("Status: BT"), 0);

    // 2. Bluetooth icon in the status icon slot on row 1.
    screen.clearStatusRow();
    screen.setStatusIcon(0, SafeIcon::BLUETOOTH);

    // 3. Connection state text next to the icon.
    if (isBluetoothConnected)
    {
        screen.setStatusText(F("Connected"));
    }
    else
    {
        screen.setStatusText(F("No connection"));
    }
}
