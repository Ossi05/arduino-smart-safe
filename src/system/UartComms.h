#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdint.h>

#include "../SafeController.h"
#include "../storage/EepromLogger.h"
#include "../storage/PasswordManager.h"

/*
 * Handles all UART communication for the smart safe.
 * Receives commands over USART0 (interrupt-driven), assembles them into
 * complete lines, and dispatches them to the appropriate handler.
 */

class UartComms
{
public:
    UartComms(SafeController &safeController,
              EepromLogger &eepromLogger,
              PasswordManager &passwordManager);

    void begin();
    void update();

private:
    SafeController &safeController;
    EepromLogger &eepromLogger;
    PasswordManager &passwordManager;

    static constexpr uint8_t LINE_BUFFER_SIZE = 64;
    char lineBuffer[LINE_BUFFER_SIZE];
    uint8_t currentLineLength;

    void sendChar(char character);
    void sendString(const char *text);
    void sendString(const __FlashStringHelper *text);
    void sendLine(const char *text); // Sends text followed by "\r\n"
    void sendLine(const __FlashStringHelper *text);
    void sendUnsignedInteger(uint16_t number);
    void handleCommandLine(char *commandLine);

    void handleUnlock(const char *arguments);
    void handleLock(const char *arguments);
    void handleStatus(const char *arguments);
    void handleLogs(const char *arguments);
    void handleClearLogs(const char *arguments);
    void handleChangePassword(const char *arguments);
    void handleResetPassword(const char *arguments);
    void handleHelp(const char *arguments);

    struct Command
    {
        const char *name;                       // Lowercase command string the user types
        const __FlashStringHelper *description; // Shown by the "help" command
        void (UartComms::*handler)(const char *arguments);
    };

    static const Command COMMANDS[];
    static const uint8_t COMMAND_COUNT;

    // Trims leading and trailing whitespace in-place.
    // Returns a pointer to the first non-space character inside the buffer.
    static char *trimWhitespace(char *inputText);

    // Returns the argument portion of a case-insensitive command match.
    // Returns nullptr if the command line does not contain the command.
    static const char *findCommandArguments(const char *commandLine,
                                            const char *commandName);
};
