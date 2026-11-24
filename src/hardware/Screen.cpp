
#include "Screen.h"

Screen::Screen(uint8_t i2cAddress,
               uint8_t columnCount,
               uint8_t rowCount)
    : i2cInterface(nullptr),
      i2cAddress(i2cAddress),
      columnCount(columnCount),
      rowCount(rowCount),
      enteredPinCode(""),
      maxPinCodeLength(4),
      isPinDigitRevealed(false),
      pinRevealStartTime(0),
      pinRevealDurationMs(600)
{
}

void Screen::begin(TwoWire &i2cInterface)
{
    this->i2cInterface = &i2cInterface;
    this->i2cInterface->begin();

    delay(50); // AIP31068L power-up delay

    sendLcdCommand(0x28); // 2-line mode, 5x8 dots
    delayMicroseconds(4500);

    sendLcdCommand(0x28);
    delayMicroseconds(150);

    sendLcdCommand(0x28);

    sendLcdCommand(0x0C); // Display ON, cursor OFF, blink OFF

    clear();

    sendLcdCommand(0x06); // Entry mode: left-to-right

    loadDefaultCustomCharacters();
}

void Screen::update()
{
    // Check whether the temporary last-digit reveal timer has expired
    if (isPinDigitRevealed &&
        (millis() - pinRevealStartTime >= pinRevealDurationMs))
    {

        isPinDigitRevealed = false;

        // Re-render the PIN with all entered digits masked
        renderPinCode(false);
    }
}

void Screen::clear()
{
    isPinDigitRevealed = false;
    enteredPinCode = "";

    sendLcdCommand(0x01);
    delayMicroseconds(2000);
}

void Screen::print(const String &text, uint8_t row)
{
    print(text.c_str(), row);
}

void Screen::print(const char *text, uint8_t row)
{
    // Cancel the active PIN reveal timer if row 0 is overwritten
    if (row == 0)
    {
        isPinDigitRevealed = false;
    }

    if (row >= rowCount)
    {
        row = rowCount - 1;
    }

    setCursorPosition(0, row);

    uint8_t textLength = text ? strlen(text) : 0;

    // Write the text and clear any remaining characters on the row
    for (uint8_t column = 0; column < columnCount; column++)
    {
        if (column < textLength)
        {
            sendLcdData(text[column]);
        }
        else
        {
            sendLcdData(' ');
        }
    }
}

void Screen::print(const __FlashStringHelper *text, uint8_t row)
{
    // Cancel the active PIN reveal timer if row 0 is overwritten
    if (row == 0)
    {
        isPinDigitRevealed = false;
    }

    if (row >= rowCount)
    {
        row = rowCount - 1;
    }

    setCursorPosition(0, row);

    const char *textPointer = reinterpret_cast<const char *>(text);
    uint8_t column = 0;
    while (column < columnCount)
    {
        char c = textPointer ? *textPointer : '\0';
        if (c != '\0')
        {
            sendLcdData(c);
            textPointer++;
        }
        else
        {
            sendLcdData(' ');
        }
        column++;
    }
}

void Screen::printPinCode(
    const char *enteredPinCode,
    uint8_t maxPinCodeLength,
    bool revealLastDigit,
    unsigned long revealDurationMs)
{
    printPinCode(String(enteredPinCode), maxPinCodeLength, revealLastDigit, revealDurationMs);
}

void Screen::printPinCode(
    const String &enteredPinCode,
    uint8_t maxPinCodeLength,
    bool revealLastDigit,
    unsigned long revealDurationMs)
{
    this->enteredPinCode = enteredPinCode;
    this->maxPinCodeLength = maxPinCodeLength;
    this->pinRevealDurationMs = revealDurationMs;

    if (revealLastDigit && this->enteredPinCode.length() > 0)
    {
        isPinDigitRevealed = true;
        pinRevealStartTime = millis();

        renderPinCode(true);
    }
    else
    {
        isPinDigitRevealed = false;

        renderPinCode(false);
    }
}

void Screen::renderPinCode(bool showLastDigit)
{
    String displayLine = "PIN: ";

    uint8_t enteredDigitCount = enteredPinCode.length();

    for (uint8_t index = 0; index < maxPinCodeLength; index++)
    {

        if (index < enteredDigitCount)
        {

            // Reveal only the newest digit while the timer is active
            if (showLastDigit && (index == enteredDigitCount - 1))
            {
                displayLine += enteredPinCode[index];
            }
            else
            {
                displayLine += '*';
            }
        }
        else
        {
            displayLine += '_';
        }
    }

    // Write directly to row 0 without resetting the reveal state
    setCursorPosition(0, 0);

    uint8_t textLength = displayLine.length();

    for (uint8_t column = 0; column < columnCount; column++)
    {
        sendLcdData(column < textLength ? displayLine[column] : ' ');
    }
}

void Screen::clearStatusRow()
{
    print("", 1);
}

void Screen::setStatusText(const String &text)
{
    setStatusText(text.c_str());
}

void Screen::setStatusText(const char *text)
{
    uint8_t startColumn = 2;

    if (startColumn >= columnCount)
    {
        return;
    }

    setCursorPosition(startColumn, 1);
    uint8_t maxCharacters = columnCount - startColumn;
    uint8_t textLength = text ? strlen(text) : 0;

    for (uint8_t index = 0;
         index < textLength && index < maxCharacters;
         index++)
    {
        sendLcdData(text[index]);
    }
}

void Screen::setStatusText(const __FlashStringHelper *text)
{
    uint8_t startColumn = 2;

    if (startColumn >= columnCount)
    {
        return;
    }

    setCursorPosition(startColumn, 1);
    uint8_t maxCharacters = columnCount - startColumn;

    const char *textPointer = reinterpret_cast<const char *>(text);
    for (uint8_t index = 0; index < maxCharacters; index++)
    {
        char c = textPointer ? *textPointer : '\0';
        if (c == '\0')
        {
            break;
        }
        sendLcdData(c);
        textPointer++;
    }
}

void Screen::setStatusIcon(uint8_t column, SafeIcon icon)
{
    if (column >= columnCount)
    {
        return;
    }

    setCursorPosition(column, 1);
    sendLcdData(static_cast<uint8_t>(icon));
}

void Screen::clearStatusIcon(uint8_t column)
{
    if (column >= columnCount)
    {
        return;
    }

    setCursorPosition(column, 1);
    sendLcdData(' ');
}

void Screen::loadDefaultCustomCharacters()
{

    // 0: Closed lock
    const uint8_t lockedIcon[8] = {
        0b01110,
        0b10001,
        0b10001,
        0b11111,
        0b11011,
        0b11011,
        0b11111,
        0b00000};

    // 1: Open lock
    const uint8_t unlockedIcon[8] = {
        0b01110,
        0b10000,
        0b10000,
        0b11111,
        0b11011,
        0b11011,
        0b11111,
        0b00000};

    // 2: Bluetooth
    const uint8_t bluetoothIcon[8] = {
        0b00100,
        0b00110,
        0b10101,
        0b01110,
        0b00100,
        0b01110,
        0b10101,
        0b00110};

    // 3: Battery
    const uint8_t batteryIcon[8] = {
        0b01110,
        0b11111,
        0b10001,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b00000};

    // 4: Alert (!)
    const uint8_t alertIcon[8] = {
        0b00100,
        0b01110,
        0b01110,
        0b01110,
        0b00100,
        0b00000,
        0b00100,
        0b00000};

    createCustomCharacter(
        static_cast<uint8_t>(SafeIcon::LOCKED), lockedIcon);

    createCustomCharacter(
        static_cast<uint8_t>(SafeIcon::UNLOCKED), unlockedIcon);

    createCustomCharacter(
        static_cast<uint8_t>(SafeIcon::BLUETOOTH), bluetoothIcon);

    createCustomCharacter(
        static_cast<uint8_t>(SafeIcon::BATTERY), batteryIcon);

    createCustomCharacter(
        static_cast<uint8_t>(SafeIcon::ALERT), alertIcon);
}

void Screen::createCustomCharacter(
    uint8_t characterLocation,
    const uint8_t characterBitmap[8])
{
    characterLocation &= 0x07;

    sendLcdCommand(0x40 | (characterLocation << 3));

    for (uint8_t row = 0; row < 8; row++)
    {
        sendLcdData(characterBitmap[row]);
    }
}

void Screen::setCursorPosition(uint8_t column, uint8_t row)
{
    const uint8_t rowOffsets[] = {0x00, 0x40};

    sendLcdCommand(0x80 | (column + rowOffsets[row]));
}

void Screen::sendLcdCommand(uint8_t command)
{
    if (!i2cInterface)
    {
        return;
    }

    i2cInterface->beginTransmission(i2cAddress);
    i2cInterface->write(0x80);
    i2cInterface->write(command);
    i2cInterface->endTransmission();
}

void Screen::sendLcdData(uint8_t data)
{
    if (!i2cInterface)
    {
        return;
    }

    i2cInterface->beginTransmission(i2cAddress);
    i2cInterface->write(0x40);
    i2cInterface->write(data);
    i2cInterface->endTransmission();
}
