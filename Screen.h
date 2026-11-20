
#pragma once
#include <Arduino.h>
#include <Wire.h>

enum class SafeIcon : uint8_t
{
    LOCKED,
    UNLOCKED,
    BLUETOOTH,
    BATTERY,
    ALERT
};

class Screen
{
private:
    // PIN rendering and masking
    void renderPinCode(bool showLastDigit);

    // LCD custom character management
    void loadDefaultCustomCharacters();
    void createCustomCharacter(
        uint8_t characterLocation,
        const uint8_t characterBitmap[8]);

    // LCD communication
    void setCursorPosition(uint8_t column, uint8_t row);
    void sendLcdCommand(uint8_t command);
    void sendLcdData(uint8_t data);

    TwoWire *i2cInterface;
    uint8_t i2cAddress;
    uint8_t columnCount;
    uint8_t rowCount;

    // PIN masking state
    String enteredPinCode;
    uint8_t maxPinCodeLength;
    bool isPinDigitRevealed;
    unsigned long pinRevealStartTime;
    unsigned long pinRevealDurationMs;

public:
    Screen(uint8_t i2cAddress = 0x3E,
           uint8_t columnCount = 16,
           uint8_t rowCount = 2);

    void begin(TwoWire &i2cInterface = Wire);
    void update();

    void clear();
    void print(const String &text, uint8_t row = 0);

    void printPinCode(
        const String &enteredPinCode,
        uint8_t maxPinCodeLength,
        bool revealLastDigit = true,
        unsigned long revealDurationMs = 600);

    void clearStatusRow();
    void setStatusText(const String &text);
    void setStatusIcon(uint8_t column, SafeIcon icon);
    void clearStatusIcon(uint8_t column);
};
