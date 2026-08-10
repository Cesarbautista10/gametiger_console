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
    bool joystickDirection[4];
    bool joystickButtonDown;
    bool joystickCommandPending;
    uint64_t joystickResponseReadyUS;
    uint64_t joystickNextCommandUS;
    uint8_t joystickFailures;

    uint8_t adcButtonDown;
    bool adcButtonCommandPending;
    uint64_t adcButtonResponseReadyUS;
    uint64_t adcButtonNextCommandUS;
    uint8_t adcButtonFailures;
    uint8_t adcButtonCandidate;
    uint8_t adcButtonCandidateCount;
    bool adcButtonArmed;
public:
    KeyBoard();
    ~KeyBoard();

    void checkKeyState(Screen *screen, bool repeatHeld = true);
private:
    void pollJoystick();
    void applyJoystickFrame(const uint8_t frame[4]);
    void releaseJoystick();
    void pollADCButtons();
    void applyADCButtonValue(uint16_t value);
    void dispatchKeys(Screen *screen, const bool keyState[KEY_COUNT],
                      bool repeatHeld);
};

#endif
