#include "common.h"
#include "display.h"
#include "screen.h"
#include "i2c_device.h"

#ifndef _GAME_TIGER_KEYBOARD_H
#define _GAME_TIGER_KEYBOARD_H

#define KEY_UP 0
#define KEY_DOWN 1
#define KEY_LEFT 2
#define KEY_RIGHT 3
#define KEY_A 4
#define KEY_B 5
#define KEY_START 6
#define KEY_SELECT 7
#define KEY_COUNT 8

// Dirección I2C de la botonera
#define DPAD_I2C_ADDR 0x56

// Rangos ADC para cada botón (12-bit ADC: 0-4095)
// Mediciones: NONE=4088, DOWN=4, LEFT=792, RIGHT=1649, UP=2349
#define ADC_RANGE_UP_MIN      2150
#define ADC_RANGE_UP_MAX      2550
#define ADC_RANGE_DOWN_MIN    0
#define ADC_RANGE_DOWN_MAX    200
#define ADC_RANGE_LEFT_MIN    600
#define ADC_RANGE_LEFT_MAX    1000
#define ADC_RANGE_RIGHT_MIN   1450
#define ADC_RANGE_RIGHT_MAX   1850
#define ADC_RANGE_NONE_MIN    3900
#define ADC_RANGE_NONE_MAX    4095

class KeyBoard
{
private:
    // Pines GPIO solo para A, B, START, SELECT (los otros 4 son I2C/ADC)
    const uint8_t pinId[KEY_COUNT] = {0, 0, 0, 0, 26, 27, 8, 9};
    bool prevKeyState[KEY_COUNT];
    bool i2c_enabled;
    uint8_t i2c_error_count;
    uint32_t last_i2c_check;
    uint32_t last_i2c_retry;  // Timestamp del último intento de reconexión
public:
    KeyBoard();
    ~KeyBoard();

    void checkKeyState(Screen *screen);
private:
    void checkI2CDPad(Screen *screen);
};

#endif