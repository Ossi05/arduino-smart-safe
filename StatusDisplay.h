
#pragma once
#include <Arduino.h>
#include "Screen.h"

enum class StatusPage : uint8_t
{
    BLUETOOTH = 0,
    STATUS_PAGE_COUNT
};

class StatusDisplay
{
private:
    static constexpr unsigned long AUTO_EXIT_TIMEOUT_MS = 5000;

    Screen &screen;
    bool isActive;
    StatusPage currentPage;
    unsigned long lastInteractionTimeMs;

    bool isBluetoothConnected;

    void renderBluetoothStatus();
    void renderCurrentPage();

public:
    StatusDisplay(Screen &screen);

    void setBluetoothConnected(bool connected);

    void activate();
    bool showNextPage(); // Returns true when the last page wraps
    void deactivate();

    bool checkAutoExitTimeout();

    bool active() const;
};
