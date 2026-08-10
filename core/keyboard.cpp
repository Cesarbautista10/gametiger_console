#include "keyboard.h"

namespace {
constexpr i2c_inst_t *I2C_PORT = i2c0;
constexpr uint8_t I2C_SDA = 24;
constexpr uint8_t I2C_SCL = 25;
constexpr uint32_t I2C_FREQUENCY = 400000;
constexpr uint32_t I2C_TIMEOUT_US = 2000;

constexpr uint8_t FRAME_COMMAND = 0x80;
constexpr uint64_t RESPONSE_DELAY_US = 10000;
constexpr uint64_t COMMAND_PERIOD_US = 10000;
constexpr uint8_t MAX_FAILURES = 4;

constexpr uint8_t JOYSTICK_ADDRESS = 0x20;
constexpr uint16_t JOYSTICK_ADC_MAX = 4095;
constexpr uint16_t JOYSTICK_LOW_PRESS = 1600;
constexpr uint16_t JOYSTICK_LOW_RELEASE = 1850;
constexpr uint16_t JOYSTICK_HIGH_RELEASE = 2245;
constexpr uint16_t JOYSTICK_HIGH_PRESS = 2495;

enum JoystickDirection : uint8_t {
    JOY_LEFT,
    JOY_RIGHT,
    JOY_UP,
    JOY_DOWN,
    JOY_DIRECTION_COUNT
};

constexpr uint8_t ADC_BUTTON_ADDRESS = 0x35;
constexpr uint16_t ADC_BUTTON_B_LIMIT = 100;
constexpr uint16_t ADC_BUTTON_START_MIN = 900;
constexpr uint16_t ADC_BUTTON_START_MAX = 1000;
constexpr uint16_t ADC_BUTTON_SELECT_MIN = 2000;
constexpr uint16_t ADC_BUTTON_SELECT_MAX = 2200;
constexpr uint16_t ADC_BUTTON_A_MIN = 3350;
constexpr uint16_t ADC_BUTTON_A_MAX = 3500;
constexpr uint16_t ADC_BUTTON_IDLE_MIN = 3900;
constexpr uint16_t ADC_BUTTON_IDLE_VALUE = 4095;

enum ADCButton : uint8_t {
    ADC_BUTTON_NONE,
    ADC_BUTTON_SELECT,
    ADC_BUTTON_START,
    ADC_BUTTON_B,
    ADC_BUTTON_A
};

constexpr bool joystickFrameValid(const uint8_t frame[4]) {
    return (frame[0] & 0x70U) == 0 && (frame[2] & 0xF0U) == 0;
}

constexpr uint8_t adcButtonFromValue(uint16_t value) {
    if (value < ADC_BUTTON_B_LIMIT)
        return ADC_BUTTON_B;
    if (value >= ADC_BUTTON_START_MIN && value <= ADC_BUTTON_START_MAX)
        return ADC_BUTTON_START;
    if (value >= ADC_BUTTON_SELECT_MIN && value <= ADC_BUTTON_SELECT_MAX)
        return ADC_BUTTON_SELECT;
    if (value >= ADC_BUTTON_A_MIN && value <= ADC_BUTTON_A_MAX)
        return ADC_BUTTON_A;
    return ADC_BUTTON_NONE;
}

static_assert(adcButtonFromValue(99) == ADC_BUTTON_B &&
              adcButtonFromValue(100) == ADC_BUTTON_NONE);
static_assert(adcButtonFromValue(900) == ADC_BUTTON_START &&
              adcButtonFromValue(1000) == ADC_BUTTON_START);
static_assert(adcButtonFromValue(2000) == ADC_BUTTON_SELECT &&
              adcButtonFromValue(2200) == ADC_BUTTON_SELECT);
static_assert(adcButtonFromValue(3350) == ADC_BUTTON_A &&
              adcButtonFromValue(3500) == ADC_BUTTON_A);
}

KeyBoard::KeyBoard()
    : prevKeyState{},
      joystickDirection{},
      joystickButtonDown(false),
      joystickCommandPending(false),
      joystickResponseReadyUS(0),
      joystickNextCommandUS(0),
      joystickFailures(0),
      adcButtonDown(ADC_BUTTON_NONE),
      adcButtonCommandPending(false),
      adcButtonResponseReadyUS(0),
      adcButtonNextCommandUS(0),
      adcButtonFailures(0),
      adcButtonCandidate(ADC_BUTTON_NONE),
      adcButtonCandidateCount(0),
      adcButtonArmed(false) {
    printf("[Keyboard] loading I2C controllers...\n");

    i2c_init(I2C_PORT, I2C_FREQUENCY);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    const uint64_t now = time_us_64();
    joystickNextCommandUS = now;
    adcButtonNextCommandUS = now;

    printf("[Keyboard] I2C0 SDA=%u SCL=%u addresses=0x%02X,0x%02X\n",
           I2C_SDA, I2C_SCL, JOYSTICK_ADDRESS, ADC_BUTTON_ADDRESS);
}

KeyBoard::~KeyBoard() {
}

void KeyBoard::releaseJoystick() {
    for (uint8_t i = 0; i < JOY_DIRECTION_COUNT; ++i)
        joystickDirection[i] = false;
    joystickButtonDown = false;
}

void KeyBoard::applyJoystickFrame(const uint8_t frame[4]) {
    const uint16_t rawX =
        (static_cast<uint16_t>(frame[0] & 0x0FU) << 8) | frame[1];
    const uint16_t rawY =
        (static_cast<uint16_t>(frame[2] & 0x0FU) << 8) | frame[3];
    const uint16_t x = JOYSTICK_ADC_MAX - rawX;
    const uint16_t y = JOYSTICK_ADC_MAX - rawY;

    // The joystick PCB axes are swapped relative to the display. Hysteresis
    // keeps ADC noise near a threshold from rapidly toggling a direction.
    joystickDirection[JOY_LEFT] =
        y < (joystickDirection[JOY_LEFT] ? JOYSTICK_LOW_RELEASE
                                         : JOYSTICK_LOW_PRESS);
    joystickDirection[JOY_RIGHT] =
        y > (joystickDirection[JOY_RIGHT] ? JOYSTICK_HIGH_RELEASE
                                          : JOYSTICK_HIGH_PRESS);
    joystickDirection[JOY_UP] =
        x < (joystickDirection[JOY_UP] ? JOYSTICK_LOW_RELEASE
                                       : JOYSTICK_LOW_PRESS);
    joystickDirection[JOY_DOWN] =
        x > (joystickDirection[JOY_DOWN] ? JOYSTICK_HIGH_RELEASE
                                         : JOYSTICK_HIGH_PRESS);
    joystickButtonDown = (frame[0] & 0x80U) == 0;
}

void KeyBoard::pollJoystick() {
    const uint64_t now = time_us_64();

    if (joystickCommandPending) {
        if (now < joystickResponseReadyUS)
            return;

        uint8_t frame[4];
        const int count = i2c_read_timeout_us(
            I2C_PORT, JOYSTICK_ADDRESS, frame, sizeof(frame), false,
            I2C_TIMEOUT_US);
        joystickCommandPending = false;

        if (count == static_cast<int>(sizeof(frame)) &&
            joystickFrameValid(frame)) {
            joystickFailures = 0;
            applyJoystickFrame(frame);
        } else if (++joystickFailures >= MAX_FAILURES) {
            releaseJoystick();
        }
    }

    if (now < joystickNextCommandUS)
        return;

    const int count = i2c_write_timeout_us(
        I2C_PORT, JOYSTICK_ADDRESS, &FRAME_COMMAND, 1, false, I2C_TIMEOUT_US);
    joystickNextCommandUS = now + COMMAND_PERIOD_US;
    if (count == 1) {
        joystickCommandPending = true;
        joystickResponseReadyUS = now + RESPONSE_DELAY_US;
    } else if (++joystickFailures >= MAX_FAILURES) {
        releaseJoystick();
    }
}

void KeyBoard::applyADCButtonValue(uint16_t value) {
    const uint8_t nextButton = adcButtonFromValue(value);

    // Ignore transient zeroes during startup until 0x35 reports idle.
    if (!adcButtonArmed) {
        if (value >= ADC_BUTTON_IDLE_MIN)
            adcButtonArmed = true;
        return;
    }

    // Presses require two equal samples. Release is immediate.
    if (nextButton == ADC_BUTTON_NONE) {
        adcButtonCandidate = ADC_BUTTON_NONE;
        adcButtonCandidateCount = 0;
    } else if (nextButton != adcButtonCandidate) {
        adcButtonCandidate = nextButton;
        adcButtonCandidateCount = 1;
        return;
    } else if (adcButtonCandidateCount < 2) {
        if (++adcButtonCandidateCount < 2)
            return;
    }

    adcButtonDown = nextButton;
}

void KeyBoard::pollADCButtons() {
    const uint64_t now = time_us_64();

    if (adcButtonCommandPending) {
        if (now < adcButtonResponseReadyUS)
            return;

        uint8_t frame[2];
        const int count = i2c_read_timeout_us(
            I2C_PORT, ADC_BUTTON_ADDRESS, frame, sizeof(frame), false,
            I2C_TIMEOUT_US);
        adcButtonCommandPending = false;

        if (count == static_cast<int>(sizeof(frame))) {
            // The working 0x35 firmware sends [low byte, high nibble].
            const uint16_t value = static_cast<uint16_t>(frame[0]) |
                (static_cast<uint16_t>(frame[1] & 0x0FU) << 8);
            adcButtonFailures = 0;
            applyADCButtonValue(value);
        } else if (++adcButtonFailures >= MAX_FAILURES) {
            applyADCButtonValue(ADC_BUTTON_IDLE_VALUE);
        }
    }

    if (now < adcButtonNextCommandUS)
        return;

    const int count = i2c_write_timeout_us(
        I2C_PORT, ADC_BUTTON_ADDRESS, &FRAME_COMMAND, 1, false,
        I2C_TIMEOUT_US);
    adcButtonNextCommandUS = now + COMMAND_PERIOD_US;
    if (count == 1) {
        adcButtonCommandPending = true;
        adcButtonResponseReadyUS = now + RESPONSE_DELAY_US;
    } else if (++adcButtonFailures >= MAX_FAILURES) {
        applyADCButtonValue(ADC_BUTTON_IDLE_VALUE);
    }
}

void KeyBoard::dispatchKeys(Screen *screen,
                            const bool keyState[KEY_COUNT],
                            bool repeatHeld) {
    for (uint8_t key = 0; key < KEY_COUNT; ++key) {
        if (keyState[key] != prevKeyState[key]) {
            if (keyState[key])
                screen->keyPressed(key);
            else
                screen->keyReleased(key);
        } else if (keyState[key] && repeatHeld) {
            screen->keyDown(key);
        }
        prevKeyState[key] = keyState[key];
    }
}

void KeyBoard::checkKeyState(Screen *screen, bool repeatHeld) {
    pollJoystick();
    pollADCButtons();

    bool keyState[KEY_COUNT] = {};
    keyState[KEY_LEFT] = joystickDirection[JOY_LEFT];
    keyState[KEY_RIGHT] = joystickDirection[JOY_RIGHT];
    keyState[KEY_UP] = joystickDirection[JOY_UP];
    keyState[KEY_DOWN] = joystickDirection[JOY_DOWN];

    // The 0x20 push switch and the 0x35 A button are both primary action.
    keyState[KEY_A] = joystickButtonDown || adcButtonDown == ADC_BUTTON_A;
    keyState[KEY_B] = adcButtonDown == ADC_BUTTON_B;
    keyState[KEY_START] = adcButtonDown == ADC_BUTTON_START;
    keyState[KEY_SELECT] = adcButtonDown == ADC_BUTTON_SELECT;

    dispatchKeys(screen, keyState, repeatHeld);
}
