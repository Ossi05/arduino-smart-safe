#pragma once
#include <Arduino.h>
#include <nrf.h>
#include "GpioPin.h"

// Arduino Nano 33 BLE header pins -> nRF52840 GPIO (port, pin), matching the
// ARDUINO_NANO33BLE variant table used by digitalPinToPinName().

enum class NanoPin : uint8_t
{
    DIGITAL_0,
    DIGITAL_1,
    DIGITAL_2,
    DIGITAL_3,
    DIGITAL_4,
    DIGITAL_5,
    DIGITAL_6,
    DIGITAL_7,
    DIGITAL_8,
    DIGITAL_9,
    DIGITAL_10,
    DIGITAL_11,
    DIGITAL_12,
    DIGITAL_13,
    ANALOG_0,
    ANALOG_1,
    ANALOG_2,
    ANALOG_3,
    ANALOG_4,
    ANALOG_5,
    ANALOG_6,
    ANALOG_7,
    TOTAL_PIN_COUNT
};

constexpr GpioPin NANO_33_BLE_PIN_MAP[] = {
    GpioPin(1, 3),  // D0  (TX)
    GpioPin(1, 10), // D1  (RX)
    GpioPin(1, 11), // D2
    GpioPin(1, 12), // D3
    GpioPin(1, 15), // D4
    GpioPin(1, 13), // D5
    GpioPin(1, 14), // D6
    GpioPin(0, 23), // D7
    GpioPin(0, 21), // D8
    GpioPin(0, 27), // D9
    GpioPin(1, 2),  // D10
    GpioPin(1, 1),  // D11 (MOSI)
    GpioPin(1, 8),  // D12 (MISO)
    GpioPin(0, 13), // D13 (SCK, on-board LED)
    GpioPin(0, 4),  // A0
    GpioPin(0, 5),  // A1
    GpioPin(0, 30), // A2
    GpioPin(0, 29), // A3
    GpioPin(0, 31), // A4  (SDA)
    GpioPin(0, 2),  // A5  (SCL)
    GpioPin(0, 28), // A6
    GpioPin(0, 3),  // A7
};

static_assert(sizeof(NANO_33_BLE_PIN_MAP) / sizeof(NANO_33_BLE_PIN_MAP[0]) ==
                  static_cast<uint8_t>(NanoPin::TOTAL_PIN_COUNT),
              "The Nano 33 BLE pin map must describe D0-D13 and A0-A7");

constexpr GpioPin getNanoGpioPin(NanoPin nanoPin)
{
    return NANO_33_BLE_PIN_MAP[static_cast<uint8_t>(nanoPin)];
}
