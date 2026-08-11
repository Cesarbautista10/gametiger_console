#include "audio.h"

Audio::Audio() {
    // Configurar GPIO para PWM
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    
    // Obtener el slice PWM del pin
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    
    // Configurar PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    
    // Iniciar con el PWM apagado
    pwm_set_gpio_level(AUDIO_PIN, 0);

    tone_active = false;
    tone_end_ms = 0;
    melody_length = 0;
    melody_index = 0;
    melody_playing = false;
    
    initialized = true;
    printf("[Audio] Initialized on GPIO %d (non-blocking)\n", AUDIO_PIN);
}

Audio::~Audio() {
    stop();
    pwm_set_enabled(slice_num, false);
}

void Audio::playToneBlocking(uint16_t frequency, uint16_t duration_ms) {
    if (!initialized || frequency == 0) return;

    uint32_t clock = 266000000 / 4; // Clock speed dividido por clkdiv
    uint32_t divider = clock / frequency;

    pwm_set_wrap(slice_num, divider - 1);
    pwm_set_gpio_level(AUDIO_PIN, divider / 2); // 50% duty cycle
    sleep_ms(duration_ms);
    pwm_set_gpio_level(AUDIO_PIN, 0);
}

void Audio::startToneInternal(uint16_t frequency, uint16_t duration_ms) {
    if (!initialized) return;

    uint32_t now = time_us_32() / 1000;
    if (frequency == 0) {
        pwm_set_gpio_level(AUDIO_PIN, 0);
        tone_active = true;
        tone_end_ms = now + duration_ms;
        return;
    }

    uint32_t clock = 266000000 / 4;
    uint32_t divider = clock / frequency;
    pwm_set_wrap(slice_num, divider - 1);
    pwm_set_gpio_level(AUDIO_PIN, divider / 2);

    tone_active = true;
    tone_end_ms = now + duration_ms;
}

void Audio::update() {
    if (!tone_active) return;

    uint32_t now = time_us_32() / 1000;
    if (now < tone_end_ms) return;

    pwm_set_gpio_level(AUDIO_PIN, 0);
    tone_active = false;

    if (melody_playing && melody_index < melody_length) {
        ToneNote &note = melody_queue[melody_index++];
        startToneInternal(note.frequency, note.duration_ms);
    } else {
        melody_playing = false;
    }
}

void Audio::playMenuSound() {
    melody_playing = false;
    startToneInternal(800, 50);
}

void Audio::playSelectSound() {
    melody_playing = false;
    startToneInternal(600, 30);
}

void Audio::playWinSound() {
    melody_queue[0] = {659, 100};
    melody_queue[1] = {784, 150};
    melody_queue[2] = {0, 50};
    melody_queue[3] = {1047, 300};
    melody_length = 4;
    melody_index = 0;
    melody_playing = true;
    startToneInternal(523, 100);
}

void Audio::playLoseSound() {
    melody_queue[0] = {349, 200};
    melody_queue[1] = {294, 200};
    melody_queue[2] = {0, 50};
    melody_queue[3] = {262, 400};
    melody_length = 4;
    melody_index = 0;
    melody_playing = true;
    startToneInternal(392, 200);
}

void Audio::stop() {
    pwm_set_gpio_level(AUDIO_PIN, 0);
    tone_active = false;
    melody_playing = false;
}
