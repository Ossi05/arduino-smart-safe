#pragma once
#include <Arduino.h>
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
private:
    struct Command
    {
        const char *name;
        const char *description;
        void (UartComms::*handler)(const char *arguments);
    };

    static constexpr uint8_t LINE_BUFFER_SIZE = 64;
    static const Command COMMANDS[];
    static const uint8_t COMMAND_COUNT;

    SafeController &safeController;
    EepromLogger &eepromLogger;
    PasswordManager &passwordManager;

    char lineBuffer[LINE_BUFFER_SIZE];
    uint8_t currentLineLength;

    void sendChar(char character);
    void sendString(const char *text);
    void sendString(const __FlashStringHelper *text);
    void sendLine(const char *text);
    void sendLine(const __FlashStringHelper *text);
    void sendUnsignedInteger(uint32_t number);
    void handleCommandLine(char *commandLine);

    void handleUnlock(const char *arguments);
    void handleLock(const char *arguments);
    void handleStatus(const char *arguments);
    void handleLogs(const char *arguments);
    void handleClearLogs(const char *arguments);
    void handleChangePassword(const char *arguments);
    void handleResetPassword(const char *arguments);
    void handleHelp(const char *arguments);

    static char *trimWhitespace(char *inputText);

    // Returns the argument portion of a case-insensitive command match.
    // Returns nullptr if the command line does not contain the command.
    static const char *findCommandArguments(const char *commandLine,
                                            const char *commandName);

public:
    UartComms(SafeController &safeController,
              EepromLogger &eepromLogger,
              PasswordManager &passwordManager);

    void begin();
    void update();
};
