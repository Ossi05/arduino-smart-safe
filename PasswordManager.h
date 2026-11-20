#pragma once
#include <Arduino.h>
#include <stdint.h>

class PasswordManager {
private:
    static constexpr uint8_t  PIN_LENGTH         = 4;
    static constexpr uint8_t  INITIALIZED_MARKER = 0x5A;
    static constexpr uint16_t MARKER_ADDRESS     = 5; // Uses reserved bytes 5..15
    static constexpr uint16_t PIN_START_ADDRESS  = 6; // Addresses 6, 7, 8, 9
    static const char DEFAULT_PIN[];

public:
    void begin();
    bool isCorrectPassword(const char* password) const;
    bool setPassword(const char* newPin);
    void resetPassword();
};