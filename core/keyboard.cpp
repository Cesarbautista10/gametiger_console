#include "keyboard.h"
#include "audio.h"

KeyBoard::KeyBoard() {
    printf("[Keyboard] driver loading...\n");
    
    // Inicializar I2C para la botonera
    i2c_device_init();
    i2c_enabled = true;
    i2c_error_count = 0;
    last_i2c_check = 0;
    last_i2c_retry = 0;
    
    // Inicializar estados previos
    for (uint8_t i = 0; i < KEY_COUNT; i++)
        this->prevKeyState[i] = false;

    // Inicializar solo los botones GPIO (A, B, START, SELECT)
    for (uint8_t i = KEY_A; i < KEY_COUNT; i++) {
        gpio_init(pinId[i]);
        gpio_set_dir(pinId[i], GPIO_IN);
        gpio_pull_up(pinId[i]);
    }
    
    printf("[Keyboard] I2C D-Pad enabled at address 0x%02X\n", DPAD_I2C_ADDR);
    printf("[Keyboard] Done\n");
}

KeyBoard::~KeyBoard() {
}

void KeyBoard::checkKeyState(Screen *screen) {
    // Verificar botones I2C del D-Pad (UP, DOWN, LEFT, RIGHT)
    if (i2c_enabled) {
        checkI2CDPad(screen);
    }
    
    // Verificar botones GPIO (A, B, START, SELECT)
    for (uint8_t i = KEY_A; i < KEY_COUNT; i++) {
        bool keyState = !gpio_get(pinId[i]);
        if (this->prevKeyState[i] != keyState) {
            if (keyState) {
                screen->keyPressed(i);
                // Sonido al presionar tecla
                if (globalAudio) {
                    globalAudio->playSelectSound();
                }
            } else
                screen->keyReleased(i);
        } else if(keyState)
            screen->keyDown(i);
        this->prevKeyState[i] = keyState;
    }
}

void KeyBoard::checkI2CDPad(Screen *screen) {
    // Usar siempre el mismo sistema de tiempo
    uint32_t now = time_us_32() / 1000;
    
    // Si I2C está deshabilitado, intentar reconectar cada 2 segundos
    if (!i2c_enabled) {
        uint32_t elapsed = now - last_i2c_retry;
        if (elapsed > 2000) {  // 2 segundos
            printf("[Keyboard] Attempting reconnection (elapsed: %u ms)...\n", elapsed);
            
            // PRIMERO: Reset completo del hardware I2C
            i2c_deinit(i2c0);
            sleep_ms(100);
            i2c_device_init();
            sleep_ms(50);
            
            // SEGUNDO: Hacer scan para verificar que el dispositivo esté presente
            printf("[Keyboard] Scanning I2C bus for device 0x56...\n");
            uint8_t dummy = 0;
            int scan_result = i2c_write_blocking(i2c0, DPAD_I2C_ADDR, &dummy, 1, false);
            
            if (scan_result >= 0) {
                printf("[Keyboard] Device found! Reconnected successfully\n");
                
                // Resetear contadores
                i2c_error_count = 0;
                i2c_enabled = true;
                last_i2c_check = now;
            } else {
                printf("[Keyboard] Device not found (ret=%d), will retry...\n", scan_result);
            }
            
            last_i2c_retry = now;  // Actualizar retry time independientemente del resultado
        }
        return;
    }
    
    // Solo verificar cada 10ms para reducir carga I2C
    if (now - last_i2c_check < 10) {
        return;
    }
    last_i2c_check = now;
    
    // Si hubo muchos errores, desactivar temporalmente
    if (i2c_error_count > 5) {
        if (i2c_enabled) {
            printf("[Keyboard] I2C disabled due to errors (count: %d)\n", i2c_error_count);
            printf("[Keyboard] Will retry in 2 seconds...\n");
            i2c_enabled = false;
            last_i2c_retry = now;  // Iniciar temporizador de retry
        }
        // Liberar todos los botones
        for (uint8_t i = KEY_UP; i <= KEY_RIGHT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
        return;
    }
    
    uint16_t adc_value = 0;
    
    // Leer valor ADC completo (12-bit) desde la botonera I2C
    if (readADC_Full(DPAD_I2C_ADDR, &adc_value)) {
        if (i2c_error_count > 0) {
            printf("[Keyboard] I2C recovered, error count was: %d\n", i2c_error_count);
        }
        i2c_error_count = 0; // Reset contador de errores en lectura exitosa
        
        // Debug: imprimir valor ADC solo cuando cambia significativamente
        static uint16_t last_adc_debug = 0;
        if (abs((int)adc_value - (int)last_adc_debug) > 100) {
            printf("[Keyboard] ADC value: %d\n", adc_value);
            last_adc_debug = adc_value;
        }
        
        // Determinar qué botón está presionado basándose en rangos ADC
        bool keyStates[4] = {false, false, false, false};
        
        if (adc_value >= ADC_RANGE_UP_MIN && adc_value <= ADC_RANGE_UP_MAX) {
            keyStates[KEY_UP] = true;
        } else if (adc_value >= ADC_RANGE_DOWN_MIN && adc_value <= ADC_RANGE_DOWN_MAX) {
            keyStates[KEY_DOWN] = true;
        } else if (adc_value >= ADC_RANGE_LEFT_MIN && adc_value <= ADC_RANGE_LEFT_MAX) {
            keyStates[KEY_LEFT] = true;
        } else if (adc_value >= ADC_RANGE_RIGHT_MIN && adc_value <= ADC_RANGE_RIGHT_MAX) {
            keyStates[KEY_RIGHT] = true;
        }
        
        // Procesar cambios de estado para cada tecla direccional
        for (uint8_t i = KEY_UP; i <= KEY_RIGHT; i++) {
            bool keyState = keyStates[i];
            
            if (this->prevKeyState[i] != keyState) {
                if (keyState) {
                    printf("[Keyboard] DPAD pressed: %d (ADC: %d)\n", i, adc_value);
                    screen->keyPressed(i);
                    // Sonido al presionar tecla
                    if (globalAudio) {
                        globalAudio->playSelectSound();
                    }
                } else {
                    screen->keyReleased(i);
                }
            } else if (keyState) {
                screen->keyDown(i);
            }
            this->prevKeyState[i] = keyState;
        }
    } else {
        i2c_error_count++;
        // No imprimir errores para evitar spam
        
        // Si falla la lectura I2C, marcar como no presionado
        for (uint8_t i = KEY_UP; i <= KEY_RIGHT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
    }
}
