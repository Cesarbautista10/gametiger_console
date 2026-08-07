#include "common.h"
#include "display.h"
#include "screen.h"

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

class KeyBoard
{
private:
    bool prevKeyState[KEY_COUNT];
    bool readActionButtons;
public:
    KeyBoard();
    ~KeyBoard();

    void checkKeyState(Screen *screen);
private:
    bool readADC(uint8_t highCommand, uint8_t lowCommand, uint16_t *value);
    int8_t decodeADC(uint16_t value, const uint8_t buttonMap[4]);
    void updateGroup(Screen *screen, uint8_t firstKey, uint8_t lastKey,
                     int8_t activeKey);
};

#endif
