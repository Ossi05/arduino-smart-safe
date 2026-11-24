#include "PasswordManager.h"
#include "EepromStorage.h"
#include <string.h>

const char PasswordManager::DEFAULT_PIN[] = "1234";

void PasswordManager::begin() {
    // If EEPROM hasn't been initialized for the PIN yet, write the default PIN
    if (EepromStorage::readByte(MARKER_ADDRESS) != INITIALIZED_MARKER) {
        resetPassword();
        EepromStorage::writeByte(MARKER_ADDRESS, INITIALIZED_MARKER);
    }
}

bool PasswordManager::isCorrectPassword(const char* attempt) const {
    if (attempt == nullptr || strlen(attempt) != PIN_LENGTH) {
        return false;
    }

    for (uint8_t i = 0; i < PIN_LENGTH; i++) {
        char storedChar = static_cast<char>(EepromStorage::readByte(PIN_START_ADDRESS + i));
        if (attempt[i] != storedChar) {
            return false;
        }
    }
    return true;
}

bool PasswordManager::setPassword(const char* newPin) {
    // Check that the pin is PIN_LENGTH characters long
    if (newPin == nullptr || strlen(newPin) != PIN_LENGTH) {
        return false;
    }
    for (uint8_t i = 0; i < PIN_LENGTH; i++) {
        if (newPin[i] < '0' || newPin[i] > '9') {
            return false; // Reject non-numeric characters
        }
    }

    // Write all PIN digits to Flash storage in a single update
    EepromStorage::writeBytes(PIN_START_ADDRESS, reinterpret_cast<const uint8_t *>(newPin), PIN_LENGTH);
    return true;
}

void PasswordManager::resetPassword() {
    setPassword(DEFAULT_PIN);
}