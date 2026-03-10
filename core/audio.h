#include "common.h"

#ifndef _GAME_TIGER_AUDIO_H
#define _GAME_TIGER_AUDIO_H

class Audio {
private:
    uint slice_num;
    bool initialized;
public:
    Audio();
    ~Audio();
    
    void playTone(uint16_t frequency, uint16_t duration_ms);
    void playMenuSound();
    void playWinSound();
    void playLoseSound();
    void playSelectSound();
    void stop();
};

#endif
