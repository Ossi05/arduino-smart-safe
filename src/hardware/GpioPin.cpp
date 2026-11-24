#include "GpioPin.h"

NRF_GPIO_Type *GpioPin::getPortRegisters() const
{
    return (portNumber == 0) ? NRF_P0 : NRF_P1;
}

uint32_t GpioPin::getPinBitMask() const
{
    return 1UL << pinNumber;
}

void GpioPin::configureAsPushPullOutput() const
{
    NRF_GPIO_Type *portRegisters = getPortRegisters();

    // 1. Select standard push-pull drive with the input buffer disconnected
    portRegisters->PIN_CNF[pinNumber] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

    // 2. Preload a LOW level before enabling the driver so the pin never glitches HIGH
    portRegisters->OUTCLR = getPinBitMask();
    portRegisters->DIRSET = getPinBitMask();
}

void GpioPin::configureAsOpenDrainOutput() const
{
    NRF_GPIO_Type *portRegisters = getPortRegisters();

    // 1. Select "standard 0, disconnect 1" drive: the pin only ever sinks current
    portRegisters->PIN_CNF[pinNumber] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                        (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

    // 2. Start released (HIGH = disconnected), then enable the driver
    portRegisters->OUTSET = getPinBitMask();
    portRegisters->DIRSET = getPinBitMask();
}

void GpioPin::configureAsInputWithPullUp() const
{
    NRF_GPIO_Type *portRegisters = getPortRegisters();

    // 1. Disable the output driver
    portRegisters->DIRCLR = getPinBitMask();

    // 2. Connect the input buffer and enable the internal pull-up resistor
    portRegisters->PIN_CNF[pinNumber] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                                        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void GpioPin::setHigh() const
{
    getPortRegisters()->OUTSET = getPinBitMask();
}

void GpioPin::setLow() const
{
    getPortRegisters()->OUTCLR = getPinBitMask();
}

bool GpioPin::isHigh() const
{
    return (getPortRegisters()->IN & getPinBitMask()) != 0;
}
