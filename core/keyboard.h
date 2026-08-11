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

// Dirección persistida configurada en la botonera de esta consola.
// 0x20 sigue siendo la dirección de fábrica y se prueba como respaldo.
#define JOYSTICK_I2C_ADDR         0x53
#define JOYSTICK_FACTORY_I2C_ADDR 0x20

// Ventanas iniciales de Y/PA0 (D-pad). Son independientes de X para que
// cada escalera se pueda calibrar con sus valores reales.
#define DPAD_ADC_SLOT_1_MIN       0
#define DPAD_ADC_SLOT_1_MAX       500
#define DPAD_ADC_SLOT_2_MIN       600
#define DPAD_ADC_SLOT_2_MAX       1500
#define DPAD_ADC_SLOT_3_MIN       1550
#define DPAD_ADC_SLOT_3_MAX       2350
#define DPAD_ADC_SLOT_4_MIN       2400
#define DPAD_ADC_SLOT_4_MAX       3300

// Ventanas iniciales de X/PA1 (B/START/A/SELECT). Hoy usan los valores de
// partida del D-pad, pero quedan separadas para ajustar la segunda placa.
#define ACTION_ADC_SLOT_1_MIN     0
#define ACTION_ADC_SLOT_1_MAX     500
#define ACTION_ADC_SLOT_2_MIN     600
#define ACTION_ADC_SLOT_2_MAX     1500
#define ACTION_ADC_SLOT_3_MIN     1550
#define ACTION_ADC_SLOT_3_MAX     2350
#define ACTION_ADC_SLOT_4_MIN     2400
#define ACTION_ADC_SLOT_4_MAX     3300

#define JOYSTICK_POLL_INTERVAL_MS 30
#define JOYSTICK_RESPONSE_DELAY_MS 10
#define JOYSTICK_RETRY_INTERVAL_MS 2000
#define JOYSTICK_MAX_ERRORS 10
#define JOYSTICK_DEBOUNCE_READS 2

class KeyBoard
{
private:
    bool prevKeyState[KEY_COUNT];
    bool i2c_enabled;
    uint8_t joystick_i2c_addr;
    uint8_t i2c_error_count;
    uint32_t last_i2c_check;
    uint32_t last_i2c_retry;
    uint32_t joystick_frame_requested_at;
    bool joystick_frame_pending;

    int8_t x_candidate_key;
    int8_t y_candidate_key;
    int8_t x_stable_key;
    int8_t y_stable_key;
    uint8_t x_candidate_count;
    uint8_t y_candidate_count;

    uint16_t last_x_debug;
    uint16_t last_y_debug;
public:
    KeyBoard();
    ~KeyBoard();

    void checkKeyState(Screen *screen);

private:
    void checkI2CJoystick(Screen *screen);
    bool findI2CJoystick();
    bool validateI2CJoystick(uint8_t address);
    int8_t decodeLadder(uint16_t adc_value, const uint8_t key_map[4],
                        const uint16_t slot_min[4],
                        const uint16_t slot_max[4]) const;
    void updateButtonBank(Screen *screen, int8_t detected_key,
                          int8_t &candidate_key, uint8_t &candidate_count,
                          int8_t &stable_key, uint16_t adc_value,
                          const char *bank_name);
    void releaseAllKeys(Screen *screen);
    void resetDebounce();
};

#endif
