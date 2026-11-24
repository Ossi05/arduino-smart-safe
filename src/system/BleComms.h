#pragma once
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <KVStore.h>
#include <kvstore_global_api.h>
#include <stdint.h>

#include "../storage/EepromLogger.h"
#include "../storage/PasswordManager.h"

class SafeController;
class Screen;

enum class BleEvent : uint8_t
{
    NONE,
    ADD_DEVICE_STARTED,
    DEVICE_ADDED,
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    ADD_DEVICE_TIMED_OUT,
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

    static constexpr const char *kvDeviceCountKey = "/kv/bt_count";
    static constexpr const char *kvDeviceKeyPrefix = "/kv/bt_dev_";

    static constexpr uint8_t maxTrustedDevices = 5;
    static constexpr uint8_t macAddressLength = 18;
    static constexpr uint8_t maxPacketSize = 20;
    static constexpr uint16_t transmitBufferSize = 1024;
    static constexpr unsigned long addDeviceWindowDurationMs = 60000UL;
    static constexpr unsigned long notifyIntervalMs = 20UL;

    static const Command commands[];
    static const uint8_t commandCount;

    BLEService nordicUartService;
    BLECharacteristic commandWriteCharacteristic;
    BLECharacteristic responseNotifyCharacteristic;

    SafeController &safeController;
    EepromLogger &eepromLogger;
    PasswordManager &passwordManager;
    Screen *screen;

    char trustedDeviceAddresses[maxTrustedDevices][macAddressLength];
    uint8_t trustedDeviceCount;

    bool isBleStarted;
    bool isAdvertisingActive;
    bool isDeviceConnected;
    bool isAddingNewDevice;
    unsigned long addDeviceWindowStartMs;
    unsigned long lastNotifyTimeMs;
    BleEvent pendingEvent;

    uint8_t transmitBuffer[transmitBufferSize];
    uint16_t transmitReadIndex;
    uint16_t transmitWriteIndex;
    uint16_t queuedByteCount;

    // Flash storage helpers
    void loadTrustedDevices();
    bool saveTrustedDevice(const char *deviceAddress);
    bool isDeviceTrusted(const char *deviceAddress) const;

    // State and protocol helpers
    void updateConnectionState();
    void checkAddDeviceWindowTimeout();
    void readIncomingCommand();
    void transmitNextChunk();

    void stopAdvertising();
    void clearTransmitBuffer();
    void queueResponseLine(const char *text);
    void handleCommandLine(char *commandLine);

    // Command handlers
    void handleUnlockCommand(const char *arguments);
    void handleLockCommand(const char *arguments);
    void handleStatusCommand(const char *arguments);
    void handleLogsCommand(const char *arguments);
    void handleClearLogsCommand(const char *arguments);
    void handleResetPasswordCommand(const char *arguments);
    void handleClearBtCommand(const char *arguments);
    void handleHelpCommand(const char *arguments);

    static char *trimWhitespace(char *inputText);
    static const char *findCommandArguments(const char *commandLine,
                                            const char *commandName);

public:
    BleComms(SafeController &safeController,
             EepromLogger &eepromLogger,
             PasswordManager &passwordManager,
             Screen *screen = nullptr);

    bool begin();
    void update();

    void startAddDeviceWindow();
    void cancelAddDeviceWindow();
    void clearTrustedDevices();

    bool isAddingNewDeviceMode() const;
    bool isCentralConnected() const;
    uint8_t getRemainingAddDeviceSeconds() const;
    uint8_t getTrustedDeviceCount() const;
    BleEvent consumePendingEvent();
};
