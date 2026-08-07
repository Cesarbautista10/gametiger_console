#include "keyboard.h"

namespace {
constexpr i2c_inst_t *I2C_PORT = i2c0;
constexpr uint8_t I2C_SDA = 8;
constexpr uint8_t I2C_SCL = 9;
constexpr uint8_t CONTROLLER_ADDRESS = 0x53;

constexpr uint8_t CMD_ADC0_HIGH = 0x56;
constexpr uint8_t CMD_ADC0_LOW = 0x57;
constexpr uint8_t CMD_ADC1_HIGH = 0xD8;
constexpr uint8_t CMD_ADC1_LOW = 0xD9;

constexpr uint8_t dpadMap[4] = {KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_UP};
constexpr uint8_t actionMap[4] = {KEY_START, KEY_B, KEY_A, KEY_SELECT};
}

KeyBoard::KeyBoard() : readActionButtons(false) {
    printf("[Keyboard] loading I2C controller...\n");

    for (uint8_t i = 0; i < KEY_COUNT; ++i)
        prevKeyState[i] = false;

    i2c_init(I2C_PORT, 100000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    printf("[Keyboard] I2C0 SDA=%u SCL=%u address=0x%02X\n",
           I2C_SDA, I2C_SCL, CONTROLLER_ADDRESS);
}

KeyBoard::~KeyBoard() {
}

bool KeyBoard::readADC(uint8_t highCommand, uint8_t lowCommand,
                       uint16_t *value) {
    uint8_t high = 0;
    uint8_t low = 0;

    absolute_time_t timeout = make_timeout_time_ms(20);
    if (i2c_write_blocking_until(I2C_PORT, CONTROLLER_ADDRESS,
                                 &highCommand, 1, false, timeout) != 1)
        return false;
    sleep_ms(3);
    timeout = make_timeout_time_ms(20);
    if (i2c_read_blocking_until(I2C_PORT, CONTROLLER_ADDRESS,
                                &high, 1, false, timeout) != 1)
        return false;

    timeout = make_timeout_time_ms(20);
    if (i2c_write_blocking_until(I2C_PORT, CONTROLLER_ADDRESS,
                                 &lowCommand, 1, false, timeout) != 1)
        return false;
    sleep_ms(3);
    timeout = make_timeout_time_ms(20);
    if (i2c_read_blocking_until(I2C_PORT, CONTROLLER_ADDRESS,
                                &low, 1, false, timeout) != 1)
        return false;

    *value = (static_cast<uint16_t>(high) << 8 | low) & 0x0FFF;
    return true;
}

int8_t KeyBoard::decodeADC(uint16_t value, const uint8_t buttonMap[4]) {
    if (value <= 500)
        return buttonMap[0];
    if (value >= 600 && value <= 1500)
        return buttonMap[1];
    if (value >= 1550 && value <= 2350)
        return buttonMap[2];
    if (value >= 2400 && value <= 3300)
        return buttonMap[3];
    return -1;
}

void KeyBoard::updateGroup(Screen *screen, uint8_t firstKey, uint8_t lastKey,
                           int8_t activeKey) {
    for (uint8_t key = firstKey; key <= lastKey; ++key) {
        bool pressed = key == activeKey;
        if (pressed != prevKeyState[key]) {
            if (pressed)
                screen->keyPressed(key);
            else
                screen->keyReleased(key);
        } else if (pressed) {
            screen->keyDown(key);
        }
        prevKeyState[key] = pressed;
    }
}

void KeyBoard::checkKeyState(Screen *screen) {
    uint16_t value = 0;

    if (readActionButtons) {
        int8_t key = readADC(CMD_ADC1_HIGH, CMD_ADC1_LOW, &value)
                         ? decodeADC(value, actionMap)
                         : -1;
        updateGroup(screen, KEY_A, KEY_SELECT, key);
    } else {
        int8_t key = readADC(CMD_ADC0_HIGH, CMD_ADC0_LOW, &value)
                         ? decodeADC(value, dpadMap)
                         : -1;
        updateGroup(screen, KEY_UP, KEY_RIGHT, key);
    }

    readActionButtons = !readActionButtons;
}
