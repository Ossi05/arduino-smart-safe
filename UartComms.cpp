#include "UartComms.h"
#include <string.h>
#include <ctype.h>

// Serial owns the USART0 interrupt and receive buffer through HardwareSerial.
static constexpr uint32_t BAUD_RATE = 9600UL;

static const char DESC_UNLOCK[] PROGMEM = "Unlock the safe";
static const char DESC_LOCK[] PROGMEM = "Lock the safe";
static const char DESC_STATUS[] PROGMEM = "Report current lock state";
static const char DESC_LOGS[] PROGMEM = "Print all stored log events";
static const char DESC_CLEAR_LOGS[] PROGMEM = "Erase all stored log events";
static const char DESC_RESET_PW[] PROGMEM = "Reset PIN to default, or set a new one (e.g. reset password 1234)";
static const char DESC_HELP[] PROGMEM = "List all available commands";

const UartComms::Command UartComms::COMMANDS[] =
    {
        {"unlock", reinterpret_cast<const __FlashStringHelper *>(DESC_UNLOCK), &UartComms::handleUnlock},
        {"lock", reinterpret_cast<const __FlashStringHelper *>(DESC_LOCK), &UartComms::handleLock},
        {"status", reinterpret_cast<const __FlashStringHelper *>(DESC_STATUS), &UartComms::handleStatus},
        {"logs", reinterpret_cast<const __FlashStringHelper *>(DESC_LOGS), &UartComms::handleLogs},
        {"clear logs", reinterpret_cast<const __FlashStringHelper *>(DESC_CLEAR_LOGS), &UartComms::handleClearLogs},
        {"reset password", reinterpret_cast<const __FlashStringHelper *>(DESC_RESET_PW), &UartComms::handleResetPassword},
        {"help", reinterpret_cast<const __FlashStringHelper *>(DESC_HELP), &UartComms::handleHelp},
};

const uint8_t UartComms::COMMAND_COUNT =
    sizeof(UartComms::COMMANDS) / sizeof(UartComms::COMMANDS[0]);

UartComms::UartComms(SafeController &safeController,
                     EepromLogger &eepromLogger,
                     PasswordManager &passwordManager)
    : safeController(safeController),
      eepromLogger(eepromLogger),
      passwordManager(passwordManager),
      currentLineLength(0)
{
    lineBuffer[0] = '\0';
}

void UartComms::begin()
{
    Serial.begin(BAUD_RATE);

    sendLine(F("=== Smart Safe UART ready ==="));
    sendLine(F("Type 'help' for a list of commands."));
}

void UartComms::update()
{
    // 1. Read all available characters into complete command lines.
    while (Serial.available() > 0)
    {
        char receivedCharacter = static_cast<char>(Serial.read());

        if (receivedCharacter == '\r')
        {
            continue;
        }

        if (receivedCharacter == '\n')
        {
            lineBuffer[currentLineLength] = '\0';
            char *trimmedCommandLine = trimWhitespace(lineBuffer);

            // 2. Trim and dispatch non-empty command lines.
            if (trimmedCommandLine[0] != '\0')
            {
                handleCommandLine(trimmedCommandLine);
            }

            currentLineLength = 0;
            lineBuffer[0] = '\0';
        }
        else
        {
            if (currentLineLength < LINE_BUFFER_SIZE - 1)
            {
                lineBuffer[currentLineLength] = receivedCharacter;
                currentLineLength++;
            }
        }
    }
}

void UartComms::sendChar(char character)
{
    Serial.write(static_cast<uint8_t>(character));
}

void UartComms::sendString(const char *text)
{
    while (*text != '\0')
    {
        sendChar(*text);
        text++;
    }
}

void UartComms::sendString(const __FlashStringHelper *text)
{
    Serial.print(text);
}

void UartComms::sendLine(const char *text)
{
    sendString(text);
    sendChar('\r');
    sendChar('\n');
}

void UartComms::sendLine(const __FlashStringHelper *text)
{
    Serial.println(text);
}

void UartComms::sendUnsignedInteger(uint16_t number)
{
    if (number >= 10)
    {
        sendUnsignedInteger(static_cast<uint16_t>(number / 10));
    }
    sendChar(static_cast<char>('0' + (number % 10)));
}

void UartComms::handleCommandLine(char *commandLine)
{
    for (uint8_t commandIndex = 0; commandIndex < COMMAND_COUNT; commandIndex++)
    {
        const char *arguments =
            findCommandArguments(commandLine, COMMANDS[commandIndex].name);

        if (arguments != nullptr)
        {
            while (*arguments == ' ')
            {
                arguments++;
            }

            (this->*COMMANDS[commandIndex].handler)(arguments);
            return;
        }
    }

    sendString(F("Unknown command: '"));
    sendString(commandLine);
    sendLine(F("'  (type 'help' for a list)"));
}

void UartComms::handleUnlock(const char * /*arguments*/)
{
    safeController.remoteUnlock();
    sendLine(F("[safe] Unlocked."));
}

void UartComms::handleLock(const char * /*arguments*/)
{
    safeController.remoteLock();
    sendLine(F("[safe] Locked."));
}

void UartComms::handleStatus(const char * /*arguments*/)
{
    if (safeController.isLocked())
    {
        sendLine(F("[status] LOCKED"));
    }
    else
    {
        sendLine(F("[status] UNLOCKED"));
    }
}

void UartComms::handleLogs(const char * /*arguments*/)
{
    LogEvent storedEvents[EepromLogger::MAX_LOGS_AMT];
    uint16_t eventCount = eepromLogger.getAllEvents(storedEvents, EepromLogger::MAX_LOGS_AMT);

    if (eventCount == 0)
    {
        sendLine(F("[logs] No events stored."));
        return;
    }

    sendString(F("[logs] "));
    sendUnsignedInteger(eventCount);
    sendLine(F(" event(s):"));

    for (uint16_t eventIndex = 0; eventIndex < eventCount; eventIndex++)
    {
        sendChar('[');
        sendUnsignedInteger(storedEvents[eventIndex].timestampSec);
        sendString(F("s] "));
        sendLine(storedEvents[eventIndex].getEventName());
    }
}

void UartComms::handleClearLogs(const char * /*arguments*/)
{
    eepromLogger.clearLogs();
    sendLine(F("[logs] All logs cleared."));
}

void UartComms::handleResetPassword(const char *arguments)
{
    if (arguments == nullptr || arguments[0] == '\0')
    {
        passwordManager.resetPassword();
        eepromLogger.logEvent(LogEventType::PIN_CHANGED);
        sendLine(F("[password] Reset to default PIN."));
        return;
    }

    static constexpr uint8_t REQUIRED_PIN_LENGTH = 4;

    uint8_t pinLength = 0;
    const char *pinCharacter = arguments;

    while (*pinCharacter != '\0')
    {
        if (!isdigit(static_cast<unsigned char>(*pinCharacter)))
        {
            sendLine(F("[password] Error: PIN must contain digits only."));
            return;
        }
        pinLength++;
        pinCharacter++;
    }

    if (pinLength != REQUIRED_PIN_LENGTH)
    {
        sendLine(F("[password] Error: PIN must be exactly 4 digits."));
        return;
    }

    if (passwordManager.setPassword(arguments))
    {
        eepromLogger.logEvent(LogEventType::PIN_CHANGED);
        sendLine(F("[password] PIN changed successfully."));
    }
    else
    {
        sendLine(F("[password] Error: Failed to save new PIN."));
    }
}

void UartComms::handleHelp(const char * /*arguments*/)
{
    sendLine(F("Available commands:"));

    for (uint8_t commandIndex = 0; commandIndex < COMMAND_COUNT; commandIndex++)
    {
        sendString(F("  "));
        sendString(COMMANDS[commandIndex].name);
        sendString(F("  --  "));
        sendLine(COMMANDS[commandIndex].description);
    }
}

char *UartComms::trimWhitespace(char *inputText)
{
    while (*inputText != '\0' &&
           isspace(static_cast<unsigned char>(*inputText)))
    {
        inputText++;
    }

    char *trimmedEnd = inputText + strlen(inputText);
    while (trimmedEnd > inputText &&
           isspace(static_cast<unsigned char>(*(trimmedEnd - 1))))
    {
        trimmedEnd--;
    }
    *trimmedEnd = '\0';

    return inputText;
}

const char *UartComms::findCommandArguments(const char *commandLine,
                                            const char *commandName)
{
    const char *lineCharacter = commandLine;
    const char *commandCharacter = commandName;

    while (*commandCharacter != '\0')
    {
        if (tolower(static_cast<unsigned char>(*lineCharacter)) !=
            tolower(static_cast<unsigned char>(*commandCharacter)))
        {
            return nullptr;
        }
        lineCharacter++;
        commandCharacter++;
    }

    if (*lineCharacter == '\0' || *lineCharacter == ' ')
    {
        return lineCharacter;
    }

    return nullptr;
}
