#pragma once
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <stdint.h>

#include "../SafeController.h"
#include "../storage/EepromLogger.h"
#include "../storage/PasswordManager.h"

enum class BleEvent : uint8_t
{
    NONE,
    PAIRING_STARTED,
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    PAIRING_TIMED_OUT,
};

class BleComms
{
private:
    struct Command
    {
        const char *name;
        const char *description;
        void (BleComms::*handler)(const char *arguments);
    };

    static constexpr const char *localName = "SmartSafe";
    static constexpr const char *serviceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
    static constexpr const char *commandWriteUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
    static constexpr const char *responseNotifyUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

    static constexpr uint8_t maxPacketSize = 20;
    static constexpr uint16_t transmitBufferSize = 1024;
    static constexpr unsigned long pairingWindowDurationMs = 60000UL;
    static constexpr unsigned long notifyIntervalMs = 20UL;

    static const Command commands[];
    static const uint8_t commandCount;

    BLEService nordicUartService;
    BLECharacteristic commandWriteCharacteristic;
    BLECharacteristic responseNotifyCharacteristic;

    SafeController &safeController;
    EepromLogger &eepromLogger;
    PasswordManager &passwordManager;

    bool isBleStarted;
    bool isAdvertisingActive;
    bool isDeviceConnected;
    unsigned long pairingWindowStartMs;
    unsigned long lastNotifyTimeMs;
    BleEvent pendingEvent;

    uint8_t transmitBuffer[transmitBufferSize];
    uint16_t transmitReadIndex;
    uint16_t transmitWriteIndex;
    uint16_t queuedByteCount;

    void updateConnectionState();
    void checkPairingWindowTimeout();
    void readIncomingCommand();
    void transmitNextChunk();

    void stopAdvertising();
    void clearTransmitBuffer();
    void queueResponseLine(const char *text);
    void handleCommandLine(char *commandLine);

    void handleUnlockCommand(const char *arguments);
    void handleLockCommand(const char *arguments);
    void handleStatusCommand(const char *arguments);
    void handleLogsCommand(const char *arguments);
    void handleClearLogsCommand(const char *arguments);
    void handleResetPasswordCommand(const char *arguments);
    void handleHelpCommand(const char *arguments);

    static char *trimWhitespace(char *inputText);
    static const char *findCommandArguments(const char *commandLine,
                                            const char *commandName);

public:
    BleComms(SafeController &safeController,
             EepromLogger &eepromLogger,
             PasswordManager &passwordManager);

    bool begin();
    void update();

    void startNewPairingWindow();
    void cancelPairingWindow();

    bool isPairingWindowOpen() const;
    bool isCentralConnected() const;
    uint8_t getRemainingPairingSeconds() const;
    BleEvent consumePendingEvent();
};
