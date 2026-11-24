#pragma once
#include <Arduino.h>
#include <nrf.h>

class GpioPin
{
private:
    uint8_t portNumber;
    uint8_t pinNumber;

    NRF_GPIO_Type *getPortRegisters() const;
    uint32_t getPinBitMask() const;

public:
    constexpr GpioPin(uint8_t portNumber, uint8_t pinNumber)
        : portNumber(portNumber),
          pinNumber(pinNumber)
    {
    }

    constexpr uint8_t getPortNumber() const { return portNumber; }
    constexpr uint8_t getPinNumber() const { return pinNumber; }

    void configureAsPushPullOutput() const;
    void configureAsOpenDrainOutput() const;
    void configureAsInputWithPullUp() const;

    void setHigh() const;
    void setLow() const;
    bool isHigh() const;
};
