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

    // Non-blocking tone state
    bool tone_active;
    uint32_t tone_end_ms;

    // Melody queue for multi-note sequences
    ToneNote melody_queue[16];
    uint8_t melody_length;
    uint8_t melody_index;
    bool melody_playing;

    void startToneInternal(uint16_t frequency, uint16_t duration_ms);
public:
    Audio();
    ~Audio();

    // Blocking - solo usar en startup antes del main loop
    void playToneBlocking(uint16_t frequency, uint16_t duration_ms);

    // Non-blocking - usar durante el juego
    void update();  // Llamar cada frame desde el main loop
    void playMenuSound();
    void playWinSound();
    void playLoseSound();
    void playSelectSound();
    void stop();
    bool isPlaying() const { return tone_active || melody_playing; }
};

#endif
