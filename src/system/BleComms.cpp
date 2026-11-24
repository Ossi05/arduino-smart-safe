#include "BleComms.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

const BleComms::Command BleComms::commands[] =
    {
        {"unlock", "Unlock the safe", &BleComms::handleUnlockCommand},
        {"lock", "Lock the safe", &BleComms::handleLockCommand},
        {"status", "Report lock state", &BleComms::handleStatusCommand},
        {"logs", "Print stored logs", &BleComms::handleLogsCommand},
        {"clear logs", "Erase stored logs", &BleComms::handleClearLogsCommand},
        {"reset password", "Reset PIN, or: reset password 1234", &BleComms::handleResetPasswordCommand},
        {"help", "List commands", &BleComms::handleHelpCommand},
};

const uint8_t BleComms::commandCount = sizeof(BleComms::commands) / sizeof(BleComms::commands[0]);

BleComms::BleComms(SafeController &safeController,
                   EepromLogger &eepromLogger,
                   PasswordManager &passwordManager)
    :     nordicUartService(serviceUuid),
    commandWriteCharacteristic(commandWriteUuid, BLEWrite | BLEWriteWithoutResponse, maxPacketSize),
    responseNotifyCharacteristic(responseNotifyUuid, BLENotify, maxPacketSize),
      safeController(safeController),
      eepromLogger(eepromLogger),
      passwordManager(passwordManager),
      isBleStarted(false),
      isAdvertisingActive(false),
      isDeviceConnected(false),
      pairingWindowStartMs(0),
      lastNotifyTimeMs(0),
      pendingEvent(BleEvent::NONE),
      transmitReadIndex(0),
      transmitWriteIndex(0),
      queuedByteCount(0)
{
}

bool BleComms::begin()
{
    // 1. Start the BLE stack
    if (!BLE.begin())
    {
        isBleStarted = false;
        return false;
    }

    // 2. Publish the device name and the Nordic UART Service
    BLE.setLocalName(localName);
    BLE.setDeviceName(localName);
    BLE.setAdvertisedService(nordicUartService);
    nordicUartService.addCharacteristic(commandWriteCharacteristic);
    nordicUartService.addCharacteristic(responseNotifyCharacteristic);
    BLE.addService(nordicUartService);

    // 3. Stay invisible until a pairing window is requested
    stopAdvertising();
    isBleStarted = true;
    return true;
}

void BleComms::update()
{
    if (!isBleStarted)
    {
        return;
    }

    // 1. Process BLE stack events without blocking
    BLE.poll();

    // 2. Track connection state, pairing timeout, incoming commands and outgoing responses
    updateConnectionState();
    checkPairingWindowTimeout();
    readIncomingCommand();
    transmitNextChunk();
}

void BleComms::startNewPairingWindow()
{
    if (!isBleStarted)
    {
        return;
    }

    // 1. Drop the current device before opening a new window.
    BLEDevice central = BLE.central();
    if (central && central.connected())
    {
        central.disconnect();
    }
    isDeviceConnected = false;
    clearTransmitBuffer();

    // 2. Open the discoverable window and start the 60-second timeout.
    BLE.stopAdvertise();
    BLE.advertise();
    isAdvertisingActive = true;
    pairingWindowStartMs = millis();
    pendingEvent = BleEvent::PAIRING_STARTED;
}

void BleComms::cancelPairingWindow()
{
    stopAdvertising();
}

bool BleComms::isPairingWindowOpen() const
{
    return isAdvertisingActive;
}

bool BleComms::isCentralConnected() const
{
    return isDeviceConnected;
}

uint8_t BleComms::getRemainingPairingSeconds() const
{
    unsigned long elapsedMs = millis() - pairingWindowStartMs;
    if (!isAdvertisingActive || elapsedMs >= pairingWindowDurationMs)
    {
        return 0;
    }

    return static_cast<uint8_t>((pairingWindowDurationMs - elapsedMs + 999UL) / 1000UL);
}

BleEvent BleComms::consumePendingEvent()
{
    BleEvent event = pendingEvent;
    pendingEvent = BleEvent::NONE;
    return event;
}

void BleComms::updateConnectionState()
{
    BLEDevice central = BLE.central();
    bool isCentralStillConnected = central && central.connected();

    // 1. New device connected: close the window so nobody else can connect.
    if (isCentralStillConnected && !isDeviceConnected)
    {
        isDeviceConnected = true;
        stopAdvertising();
        clearTransmitBuffer();
        pendingEvent = BleEvent::DEVICE_CONNECTED;
        return;
    }

    // 2. Device disconnected: remain non-discoverable until 'B' opens another window.
    if (!isCentralStillConnected && isDeviceConnected)
    {
        isDeviceConnected = false;
        clearTransmitBuffer();
        stopAdvertising();
        pendingEvent = BleEvent::DEVICE_DISCONNECTED;
    }
}

void BleComms::checkPairingWindowTimeout()
{
    if (!isAdvertisingActive)
    {
        return;
    }

    if (millis() - pairingWindowStartMs >= pairingWindowDurationMs)
    {
        stopAdvertising();
        pendingEvent = BleEvent::PAIRING_TIMED_OUT;
    }
}

void BleComms::readIncomingCommand()
{
    if (!commandWriteCharacteristic.written() || !isDeviceConnected)
    {
        return;
    }

    // 1. Every command fits in one 20-byte packet, so one write is one command line
    uint8_t receivedBytes[maxPacketSize + 1];
    int receivedLength = commandWriteCharacteristic.valueLength();
    if (receivedLength <= 0)
    {
        return;
    }
    if (receivedLength > maxPacketSize)
    {
        receivedLength = maxPacketSize;
    }
    commandWriteCharacteristic.readValue(receivedBytes, receivedLength);
    receivedBytes[receivedLength] = '\0';

    // 2. Trim and dispatch non-empty command lines
    char *trimmedCommandLine = trimWhitespace(reinterpret_cast<char *>(receivedBytes));
    if (trimmedCommandLine[0] != '\0')
    {
        handleCommandLine(trimmedCommandLine);
    }
}

void BleComms::transmitNextChunk()
{
    if (!isDeviceConnected || queuedByteCount == 0)
    {
        return;
    }

    if (millis() - lastNotifyTimeMs < notifyIntervalMs)
    {
        return;
    }

    // 1. Copy up to one packet out of the ring buffer
    uint8_t chunkLength = (queuedByteCount < maxPacketSize) ? static_cast<uint8_t>(queuedByteCount) : maxPacketSize;
    uint8_t chunkBytes[maxPacketSize];
    for (uint8_t byteIndex = 0; byteIndex < chunkLength; byteIndex++)
    {
        chunkBytes[byteIndex] = transmitBuffer[transmitReadIndex];
        transmitReadIndex = (transmitReadIndex + 1) % transmitBufferSize;
    }
    queuedByteCount -= chunkLength;

    // 2. Notify the connected device
    responseNotifyCharacteristic.writeValue(chunkBytes, chunkLength);
    lastNotifyTimeMs = millis();
}

void BleComms::stopAdvertising()
{
    BLE.stopAdvertise();
    isAdvertisingActive = false;
    pairingWindowStartMs = 0;
}

void BleComms::clearTransmitBuffer()
{
    transmitReadIndex = 0;
    transmitWriteIndex = 0;
    queuedByteCount = 0;
}

void BleComms::queueResponseLine(const char *text)
{
    uint16_t textLength = static_cast<uint16_t>(strlen(text));
    uint16_t requiredBytes = textLength + 2;

    // 1. Drop the whole line rather than sending a truncated one
    if (!isDeviceConnected || requiredBytes > (transmitBufferSize - queuedByteCount))
    {
        return;
    }

    // 2. Append the text followed by CR LF
    for (uint16_t byteIndex = 0; byteIndex < requiredBytes; byteIndex++)
    {
        uint8_t byteToQueue = (byteIndex < textLength) ? static_cast<uint8_t>(text[byteIndex])
                                                       : static_cast<uint8_t>(byteIndex == textLength ? '\r' : '\n');
        transmitBuffer[transmitWriteIndex] = byteToQueue;
        transmitWriteIndex = (transmitWriteIndex + 1) % transmitBufferSize;
    }
    queuedByteCount += requiredBytes;
}

void BleComms::handleCommandLine(char *commandLine)
{
    for (uint8_t commandIndex = 0; commandIndex < commandCount; commandIndex++)
    {
        const char *arguments = findCommandArguments(commandLine, commands[commandIndex].name);

        if (arguments != nullptr)
        {
            while (*arguments == ' ')
            {
                arguments++;
            }

            (this->*commands[commandIndex].handler)(arguments);
            return;
        }
    }

    queueResponseLine("Unknown command. Type: help");
}

void BleComms::handleUnlockCommand(const char * /*arguments*/)
{
    safeController.unlock();
    queueResponseLine("[safe] Unlocked.");
}

void BleComms::handleLockCommand(const char * /*arguments*/)
{
    safeController.lock();
    queueResponseLine("[safe] Locked.");
}

void BleComms::handleStatusCommand(const char * /*arguments*/)
{
    queueResponseLine(safeController.isLocked() ? "[status] LOCKED" : "[status] UNLOCKED");
}

void BleComms::handleLogsCommand(const char * /*arguments*/)
{
    LogEvent storedEvents[EepromLogger::MAX_LOGS_AMT];
    uint16_t eventCount = eepromLogger.getAllEvents(storedEvents, EepromLogger::MAX_LOGS_AMT);

    if (eventCount == 0)
    {
        queueResponseLine("[logs] No events.");
        return;
    }

    char headerLine[32];
    snprintf(headerLine, sizeof(headerLine), "[logs] %u event(s):", static_cast<unsigned>(eventCount));
    queueResponseLine(headerLine);

    for (uint16_t eventIndex = 0; eventIndex < eventCount; eventIndex++)
    {
        String eventLine = storedEvents[eventIndex].toString();
        queueResponseLine(eventLine.c_str());
    }
}

void BleComms::handleClearLogsCommand(const char * /*arguments*/)
{
    eepromLogger.clearLogs();
    queueResponseLine("[logs] Cleared.");
}

void BleComms::handleResetPasswordCommand(const char *arguments)
{
    bool wasPasswordChanged = false;

    if (arguments == nullptr || arguments[0] == '\0')
    {
        passwordManager.resetPassword();
        wasPasswordChanged = true;
    }
    else
    {
        wasPasswordChanged = passwordManager.setPassword(arguments);
    }

    if (!wasPasswordChanged)
    {
        queueResponseLine("[password] PIN must be 4 digits.");
        return;
    }

    eepromLogger.logEvent(LogEventType::PIN_CHANGED);
    queueResponseLine("[password] PIN updated.");
}

void BleComms::handleHelpCommand(const char * /*arguments*/)
{
    char helpLine[64];

    for (uint8_t commandIndex = 0; commandIndex < commandCount; commandIndex++)
    {
        snprintf(helpLine, sizeof(helpLine), "%s - %s", commands[commandIndex].name, commands[commandIndex].description);
        queueResponseLine(helpLine);
    }
}

char *BleComms::trimWhitespace(char *inputText)
{
    while (*inputText != '\0' && isspace(static_cast<unsigned char>(*inputText)))
    {
        inputText++;
    }

    char *trimmedEnd = inputText + strlen(inputText);
    while (trimmedEnd > inputText && isspace(static_cast<unsigned char>(*(trimmedEnd - 1))))
    {
        trimmedEnd--;
    }
    *trimmedEnd = '\0';

    return inputText;
}

const char *BleComms::findCommandArguments(const char *commandLine,
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
