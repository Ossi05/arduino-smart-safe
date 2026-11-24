#include "BleComms.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../SafeController.h"
#include "../hardware/Screen.h"

const BleComms::Command BleComms::commands[] =
{
    {"unlock", "Unlock the safe", &BleComms::handleUnlockCommand},
    {"lock", "Lock the safe", &BleComms::handleLockCommand},
    {"status", "Report lock state", &BleComms::handleStatusCommand},
    {"logs", "Print stored logs", &BleComms::handleLogsCommand},
    {"clear logs", "Erase stored logs", &BleComms::handleClearLogsCommand},
    {"reset password", "Reset PIN, or: reset password 1234", &BleComms::handleResetPasswordCommand},
    {"clear bt", "Wipe all saved Bluetooth devices", &BleComms::handleClearBtCommand},
    {"help", "List commands", &BleComms::handleHelpCommand},
};

const uint8_t BleComms::commandCount = sizeof(BleComms::commands) / sizeof(BleComms::commands[0]);

BleComms::BleComms(SafeController &safeController,
                   EepromLogger &eepromLogger,
                   PasswordManager &passwordManager,
                   Screen *screen)
    : nordicUartService(serviceUuid),
      commandWriteCharacteristic(commandWriteUuid, BLEWrite | BLEWriteWithoutResponse, maxPacketSize),
      responseNotifyCharacteristic(responseNotifyUuid, BLENotify, maxPacketSize),
      safeController(safeController),
      eepromLogger(eepromLogger),
      passwordManager(passwordManager),
      screen(screen),
      trustedDeviceCount(0),
      isBleStarted(false),
      isAdvertisingActive(false),
      isDeviceConnected(false),
      isAddingNewDevice(false),
      addDeviceWindowStartMs(0),
      lastNotifyTimeMs(0),
      pendingEvent(BleEvent::NONE),
      transmitReadIndex(0),
      transmitWriteIndex(0),
      queuedByteCount(0)
{
    for (uint8_t i = 0; i < maxTrustedDevices; i++)
    {
        trustedDeviceAddresses[i][0] = '\0';
    }
}

bool BleComms::begin()
{
    // 1. Initialise the BLE hardware stack
    if (!BLE.begin())
    {
        isBleStarted = false;
        return false;
    }

    // 2. Publish device name and Nordic UART Service
    BLE.setLocalName(localName);
    BLE.setDeviceName(localName);
    BLE.setAdvertisedService(nordicUartService);
    nordicUartService.addCharacteristic(commandWriteCharacteristic);
    nordicUartService.addCharacteristic(responseNotifyCharacteristic);
    BLE.addService(nordicUartService);

    // 3. Load saved trusted MAC addresses from non-volatile Flash
    loadTrustedDevices();

    // 4. Start advertising if trusted devices exist; otherwise stay invisible
    if (trustedDeviceCount > 0)
    {
        BLE.advertise();
        isAdvertisingActive = true;
    }
    else
    {
        BLE.stopAdvertise();
        isAdvertisingActive = false;
    }

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

    // 2. Track connection state, add-device timeout, incoming commands, and outgoing responses
    updateConnectionState();
    checkAddDeviceWindowTimeout();
    readIncomingCommand();
    transmitNextChunk();
}

void BleComms::startAddDeviceWindow()
{
    if (!isBleStarted)
    {
        return;
    }

    // 1. Disconnect any currently connected device so the new device can connect
    BLEDevice central = BLE.central();
    if (central && central.connected())
    {
        central.disconnect();
    }
    isDeviceConnected = false;
    clearTransmitBuffer();

    // 2. Enable add-device mode for 60 seconds and start advertising
    isAddingNewDevice = true;
    addDeviceWindowStartMs = millis();
    BLE.stopAdvertise();
    BLE.advertise();
    isAdvertisingActive = true;

    // 3. Notify screen and queue event
    pendingEvent = BleEvent::ADD_DEVICE_STARTED;
    if (screen != nullptr)
    {
        screen->print("Add BT Device: 60s", 0);
    }
}

void BleComms::cancelAddDeviceWindow()
{
    isAddingNewDevice = false;
    addDeviceWindowStartMs = 0;

    // Stop advertising if no trusted devices are saved and no central is connected
    if (trustedDeviceCount == 0 && !isDeviceConnected)
    {
        stopAdvertising();
    }
}

void BleComms::clearTrustedDevices()
{
    // 1. Remove individual device keys from KVStore
    for (uint8_t deviceIndex = 0; deviceIndex < maxTrustedDevices; deviceIndex++)
    {
        char storageKey[16];
        snprintf(storageKey, sizeof(storageKey), "%s%u", kvDeviceKeyPrefix, deviceIndex);
        kv_remove(storageKey);
        trustedDeviceAddresses[deviceIndex][0] = '\0';
    }

    // 2. Reset device count in memory and Flash
    trustedDeviceCount = 0;
    kv_set(kvDeviceCountKey, &trustedDeviceCount, sizeof(trustedDeviceCount), 0);

    // 3. Stop advertising if not connected and not currently adding a device
    if (!isDeviceConnected && !isAddingNewDevice)
    {
        stopAdvertising();
    }
}

bool BleComms::isAddingNewDeviceMode() const
{
    return isAddingNewDevice;
}

bool BleComms::isCentralConnected() const
{
    return isDeviceConnected;
}

uint8_t BleComms::getRemainingAddDeviceSeconds() const
{
    unsigned long elapsedMs = millis() - addDeviceWindowStartMs;
    if (!isAddingNewDevice || elapsedMs >= addDeviceWindowDurationMs)
    {
        return 0;
    }

    return static_cast<uint8_t>((addDeviceWindowDurationMs - elapsedMs + 999UL) / 1000UL);
}

uint8_t BleComms::getTrustedDeviceCount() const
{
    return trustedDeviceCount;
}

BleEvent BleComms::consumePendingEvent()
{
    BleEvent event = pendingEvent;
    pendingEvent = BleEvent::NONE;
    return event;
}

void BleComms::loadTrustedDevices()
{
    // 1. Retrieve stored device count
    size_t actualByteCount = 0;
    uint8_t storedCount = 0;
    int status = kv_get(kvDeviceCountKey, &storedCount, sizeof(storedCount), &actualByteCount);

    if (status != MBED_SUCCESS || storedCount > maxTrustedDevices)
    {
        trustedDeviceCount = 0;
        return;
    }

    // 2. Read each saved MAC address
    trustedDeviceCount = 0;
    for (uint8_t deviceIndex = 0; deviceIndex < storedCount; deviceIndex++)
    {
        char storageKey[16];
        snprintf(storageKey, sizeof(storageKey), "%s%u", kvDeviceKeyPrefix, deviceIndex);

        char addressBuffer[macAddressLength];
        status = kv_get(storageKey, addressBuffer, sizeof(addressBuffer), &actualByteCount);

        if (status == MBED_SUCCESS && actualByteCount > 0)
        {
            addressBuffer[sizeof(addressBuffer) - 1] = '\0';
            strncpy(trustedDeviceAddresses[trustedDeviceCount], addressBuffer, macAddressLength);
            trustedDeviceAddresses[trustedDeviceCount][macAddressLength - 1] = '\0';
            trustedDeviceCount++;
        }
    }
}

bool BleComms::saveTrustedDevice(const char *deviceAddress)
{
    if (deviceAddress == nullptr || deviceAddress[0] == '\0')
    {
        return false;
    }

    // 1. Skip if already authorized
    if (isDeviceTrusted(deviceAddress))
    {
        return true;
    }

    // 2. If allowlist is full, shift left to evict oldest device
    uint8_t insertIndex = trustedDeviceCount;
    if (trustedDeviceCount >= maxTrustedDevices)
    {
        for (uint8_t deviceIndex = 0; deviceIndex < maxTrustedDevices - 1; deviceIndex++)
        {
            strncpy(trustedDeviceAddresses[deviceIndex], trustedDeviceAddresses[deviceIndex + 1], macAddressLength);
            char storageKey[16];
            snprintf(storageKey, sizeof(storageKey), "%s%u", kvDeviceKeyPrefix, deviceIndex);
            kv_set(storageKey, trustedDeviceAddresses[deviceIndex], strlen(trustedDeviceAddresses[deviceIndex]) + 1, 0);
        }
        insertIndex = maxTrustedDevices - 1;
        trustedDeviceCount = maxTrustedDevices;
    }
    else
    {
        trustedDeviceCount++;
    }

    // 3. Store new MAC address at insertIndex
    strncpy(trustedDeviceAddresses[insertIndex], deviceAddress, macAddressLength);
    trustedDeviceAddresses[insertIndex][macAddressLength - 1] = '\0';

    char targetKey[16];
    snprintf(targetKey, sizeof(targetKey), "%s%u", kvDeviceKeyPrefix, insertIndex);
    kv_set(targetKey, trustedDeviceAddresses[insertIndex], strlen(trustedDeviceAddresses[insertIndex]) + 1, 0);

    // 4. Update stored device count
    kv_set(kvDeviceCountKey, &trustedDeviceCount, sizeof(trustedDeviceCount), 0);
    return true;
}

bool BleComms::isDeviceTrusted(const char *deviceAddress) const
{
    if (deviceAddress == nullptr)
    {
        return false;
    }

    for (uint8_t deviceIndex = 0; deviceIndex < trustedDeviceCount; deviceIndex++)
    {
        if (strcasecmp(trustedDeviceAddresses[deviceIndex], deviceAddress) == 0)
        {
            return true;
        }
    }

    return false;
}

void BleComms::updateConnectionState()
{
    BLEDevice central = BLE.central();
    bool isCentralCurrentlyConnected = central && central.connected();

    // 1. Handle newly connected central device
    if (isCentralCurrentlyConnected && !isDeviceConnected)
    {
        String centralAddressString = central.address();
        const char *centralAddress = centralAddressString.c_str();

        // 1a. In add-device mode, save and authorize this device
        if (isAddingNewDevice)
        {
            saveTrustedDevice(centralAddress);
            isAddingNewDevice = false;
            addDeviceWindowStartMs = 0;
            isDeviceConnected = true;

            stopAdvertising();
            clearTransmitBuffer();
            pendingEvent = BleEvent::DEVICE_ADDED;

            if (screen != nullptr)
            {
                screen->print("BT Device Added", 0);
            }
            return;
        }

        // 1b. In normal mode, reject unauthorized devices immediately
        if (!isDeviceTrusted(centralAddress))
        {
            central.disconnect();

            // Resume advertising for authorized devices if applicable
            if (trustedDeviceCount > 0 && !isAdvertisingActive)
            {
                BLE.advertise();
                isAdvertisingActive = true;
            }
            return;
        }

        // 1c. Authorized central device accepted
        isDeviceConnected = true;
        stopAdvertising();
        clearTransmitBuffer();
        pendingEvent = BleEvent::DEVICE_CONNECTED;
        return;
    }

    // 2. Handle disconnected central device
    if (!isCentralCurrentlyConnected && isDeviceConnected)
    {
        isDeviceConnected = false;
        clearTransmitBuffer();
        pendingEvent = BleEvent::DEVICE_DISCONNECTED;

        // Resume advertising if trusted devices exist or add-device mode is active
        if (trustedDeviceCount > 0 || isAddingNewDevice)
        {
            BLE.advertise();
            isAdvertisingActive = true;
        }
        else
        {
            stopAdvertising();
        }
    }
}

void BleComms::checkAddDeviceWindowTimeout()
{
    if (!isAddingNewDevice)
    {
        return;
    }

    if (millis() - addDeviceWindowStartMs >= addDeviceWindowDurationMs)
    {
        isAddingNewDevice = false;
        addDeviceWindowStartMs = 0;
        pendingEvent = BleEvent::ADD_DEVICE_TIMED_OUT;

        // Stop advertising if no trusted devices are saved
        if (trustedDeviceCount == 0 && !isDeviceConnected)
        {
            stopAdvertising();
        }
    }
}

void BleComms::readIncomingCommand()
{
    if (!commandWriteCharacteristic.written() || !isDeviceConnected)
    {
        return;
    }

    // 1. Copy command packet from characteristic
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

    // 2. Trim and dispatch non-empty command line
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

    // 1. Copy up to 20 bytes from ring buffer
    uint8_t chunkLength = (queuedByteCount < maxPacketSize) ? static_cast<uint8_t>(queuedByteCount) : maxPacketSize;
    uint8_t chunkBytes[maxPacketSize];
    for (uint8_t byteIndex = 0; byteIndex < chunkLength; byteIndex++)
    {
        chunkBytes[byteIndex] = transmitBuffer[transmitReadIndex];
        transmitReadIndex = (transmitReadIndex + 1) % transmitBufferSize;
    }
    queuedByteCount -= chunkLength;

    // 2. Send chunk via notify characteristic
    responseNotifyCharacteristic.writeValue(chunkBytes, chunkLength);
    lastNotifyTimeMs = millis();
}

void BleComms::stopAdvertising()
{
    BLE.stopAdvertise();
    isAdvertisingActive = false;
    isAddingNewDevice = false;
    addDeviceWindowStartMs = 0;
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

    // 1. Drop line if buffer capacity is exceeded
    if (!isDeviceConnected || requiredBytes > (transmitBufferSize - queuedByteCount))
    {
        return;
    }

    // 2. Append text followed by CR LF
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

void BleComms::handleClearBtCommand(const char * /*arguments*/)
{
    clearTrustedDevices();
    queueResponseLine("[bt] Trusted devices cleared.");
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
