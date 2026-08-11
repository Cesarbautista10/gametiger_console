#include "common.h"

#ifndef _GAME_TIGER_AUDIO_H
#define _GAME_TIGER_AUDIO_H

struct ToneNote {
    uint16_t frequency;
    uint16_t duration_ms;
};

class Audio {
private:
    uint slice_num;
    bool initialized;

    bool tone_active;
    uint32_t tone_end_ms;

    ToneNote melody_queue[16];
    uint8_t melody_length;
    uint8_t melody_index;
    bool melody_playing;

    void startToneInternal(uint16_t frequency, uint16_t duration_ms);
public:
    Audio();
    ~Audio();

    void playToneBlocking(uint16_t frequency, uint16_t duration_ms);
    void update();
    void playMenuSound();
    void playWinSound();
    void playLoseSound();
    void playSelectSound();
    void stop();
    bool isPlaying() const { return tone_active || melody_playing; }
};

#endif
