#include "ServoLock.h"

void ServoLock::begin()
{
    // 1. Hold the servo signal (D3 = P1.12) LOW whenever the PWM is not driving it
    servoPin.configureAsPushPullOutput();

    // 2. Route NRF_PWM0 channel 0 to the servo pin and disconnect the unused channels
    NRF_PWM0->PSEL.OUT[0] = (static_cast<uint32_t>(servoPin.getPortNumber()) << PWM_PSEL_OUT_PORT_Pos) |
                            (static_cast<uint32_t>(servoPin.getPinNumber()) << PWM_PSEL_OUT_PIN_Pos) |
                            (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    for (uint8_t channelIndex = 1; channelIndex < PWM_CHANNEL_COUNT; channelIndex++)
    {
        NRF_PWM0->PSEL.OUT[channelIndex] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
    }

    // 3. Up-counter clocked at 16 MHz / 16 = 1 MHz, wrapping at 20000 ticks = 50 Hz
    NRF_PWM0->ENABLE = PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos;
    NRF_PWM0->MODE = PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos;
    NRF_PWM0->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_16 << PWM_PRESCALER_PRESCALER_Pos;
    NRF_PWM0->COUNTERTOP = static_cast<uint32_t>(PWM_PERIOD_MICROSECONDS) << PWM_COUNTERTOP_COUNTERTOP_Pos;
    NRF_PWM0->LOOP = PWM_LOOP_CNT_Disabled << PWM_LOOP_CNT_Pos;

    // 4. EasyDMA fetches one common duty-cycle value from RAM; PWM0 keeps repeating it after the sequence ends
    NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) |
                        (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    NRF_PWM0->SEQ[0].PTR = reinterpret_cast<uint32_t>(&pwmSequenceBuffer);
    NRF_PWM0->SEQ[0].CNT = 1;
    NRF_PWM0->SEQ[0].REFRESH = 0;
    NRF_PWM0->SEQ[0].ENDDELAY = 0;

    // 5. Start in the locked position
    lock();
}

void ServoLock::lock()
{
    setPulseWidth(LOCKED_PULSE_WIDTH_MICROSECONDS);
    lockedState = true;
}

void ServoLock::unlock()
{
    setPulseWidth(UNLOCKED_PULSE_WIDTH_MICROSECONDS);
    lockedState = false;
}

bool ServoLock::isLocked() const
{
    return lockedState;
}

void ServoLock::setPulseWidth(uint16_t pulseWidthMicroseconds)
{
    // 1. Clamp to safe servo limits so the horn cannot stall against its mechanical stops
    if (pulseWidthMicroseconds < MIN_PULSE_WIDTH_MICROSECONDS)
    {
        pulseWidthMicroseconds = MIN_PULSE_WIDTH_MICROSECONDS;
    }
    else if (pulseWidthMicroseconds > MAX_PULSE_WIDTH_MICROSECONDS)
    {
        pulseWidthMicroseconds = MAX_PULSE_WIDTH_MICROSECONDS;
    }

    // 2. Update the RAM sequence buffer and restart the sequence so PWM0 latches the new duty cycle
    pwmSequenceBuffer = PWM_ACTIVE_HIGH_POLARITY_FLAG | pulseWidthMicroseconds;
    NRF_PWM0->EVENTS_SEQEND[0] = 0;
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
}
