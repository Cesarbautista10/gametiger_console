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
    
    initialized = true;
    printf("[Audio] Initialized on GPIO %d\n", AUDIO_PIN);
}

Audio::~Audio() {
    stop();
    pwm_set_enabled(slice_num, false);
}

void Audio::playTone(uint16_t frequency, uint16_t duration_ms) {
    if (!initialized || frequency == 0) return;
    
    // Calcular valores para generar la frecuencia deseada
    uint32_t clock = 266000000 / 4; // Clock speed dividido por clkdiv
    uint32_t divider = clock / frequency;
    
    pwm_set_wrap(slice_num, divider - 1);
    pwm_set_gpio_level(AUDIO_PIN, divider / 2); // 50% duty cycle
    
    // Esperar la duración del tono
    sleep_ms(duration_ms);
    
    // Apagar
    pwm_set_gpio_level(AUDIO_PIN, 0);
}

void Audio::playMenuSound() {
    playTone(800, 50);  // Tono corto agudo
}

void Audio::playSelectSound() {
    playTone(600, 30);  // Tono más corto
}

void Audio::playWinSound() {
    // Melodía simple de victoria
    playTone(523, 100);  // Do
    playTone(659, 100);  // Mi
    playTone(784, 150);  // Sol
    sleep_ms(50);
    playTone(1047, 300); // Do alto
}

void Audio::playLoseSound() {
    // Melodía descendente de derrota
    playTone(392, 200);  // Sol
    playTone(349, 200);  // Fa
    playTone(294, 200);  // Re
    sleep_ms(50);
    playTone(262, 400);  // Do bajo
}

void Audio::stop() {
    pwm_set_gpio_level(AUDIO_PIN, 0);
}
