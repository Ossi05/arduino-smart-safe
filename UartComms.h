#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdint.h>

#include "SafeController.h"
#include "EepromLogger.h"
#include "PasswordManager.h"

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

    // Configures USART0 at 9600 8N1 and prints the welcome banner.
    void begin();

    // Drains the receive buffer and dispatches any complete lines as commands.
    // Call every loop() iteration.
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

    // ── Command dispatch ──────────────────────────────────────────────────────
    // Receives a fully assembled, whitespace-trimmed line and calls the matching
    // command handler.
    void handleCommandLine(char *commandLine);

    // ── Command handlers ──────────────────────────────────────────────────────
    void handleUnlock(const char *arguments);
    void handleLock(const char *arguments);
    void handleStatus(const char *arguments);
    void handleLogs(const char *arguments);
    void handleClearLogs(const char *arguments);
    void handleResetPassword(const char *arguments);
    void handleHelp(const char *arguments);

    // ── Command table
    // To add a new command: append one entry and write its handler above.
    // Multi-word commands must appear before any shorter prefix (e.g. "clear
    // logs" before a hypothetical "clear").
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
