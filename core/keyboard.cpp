#include "keyboard.h"
#include "audio.h"

KeyBoard::KeyBoard() {
    printf("[Keyboard] driver loading...\n");
    
    // Inicializar I2C para la botonera
    i2c_device_init();
    i2c_enabled = true;
    i2c_error_count = 0;
    i2c_success_count = 0;
    last_i2c_check = 0;
    last_i2c_retry = 0;
    last_health_check = 0;
    dpad_debounce_btn = -1;
    dpad_debounce_count = 0;
    
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
            printf("[Keyboard] Attempting I2C reconnection (elapsed: %u ms)...\n", elapsed);
            
            // PASO 1: Reset completo del hardware I2C
            i2c_deinit(i2c0);
            sleep_ms(100);
            i2c_device_init();
            sleep_ms(100);
            
            // PASO 2: Escanear bus I2C para verificar dispositivo
            printf("[Keyboard] Scanning I2C bus for device 0x%02X...\n", DPAD_I2C_ADDR);
            bool device_found = false;
            
            // Intentar detectar dispositivo con write de 0 bytes
            int scan_result = i2c_write_blocking(i2c0, DPAD_I2C_ADDR, NULL, 0, false);
            
            if (scan_result >= 0) {
                printf("[Keyboard] Device detected on bus!\n");
                device_found = true;
            } else {
                // Intentar método alternativo: write con dummy byte
                uint8_t dummy = 0;
                scan_result = i2c_write_blocking(i2c0, DPAD_I2C_ADDR, &dummy, 1, false);
                if (scan_result >= 0) {
                    printf("[Keyboard] Device detected (alt method)!\n");
                    device_found = true;
                }
            }
            
            // PASO 3: Intentar lectura real si dispositivo fue detectado
            if (device_found) {
                sleep_ms(50);
                uint16_t test_value = 0;
                if (readADC_Full(DPAD_I2C_ADDR, &test_value)) {
                    printf("[Keyboard] I2C reconnected successfully! ADC test: %d\n", test_value);
                    i2c_error_count = 0;
                    i2c_enabled = true;
                    last_i2c_check = now;
                } else {
                    printf("[Keyboard] Device found but ADC read failed, will retry...\n");
                }
            } else {
                printf("[Keyboard] Device not found (ret=%d), will retry...\n", scan_result);
            }
            
            last_i2c_retry = now;
        }
        return;
    }
    
    // Solo verificar cada 30ms para reducir carga I2C y dar tiempo al dispositivo
    if (now - last_i2c_check < 30) {
        return;
    }
    last_i2c_check = now;
    
    // Si hubo muchos errores consecutivos, desactivar temporalmente
    if (i2c_error_count > 30) {
        if (i2c_enabled) {
            printf("[Keyboard] I2C disabled due to errors (count: %d)\n", i2c_error_count);
            printf("[Keyboard] Will retry reconnection in 2 seconds...\n");
            i2c_enabled = false;
            last_i2c_retry = now;
        }
        // Liberar todos los botones del D-Pad
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
        i2c_success_count++;
        
        // Si nos recuperamos de errores, notificar
        if (i2c_error_count > 3) {
            printf("[Keyboard] I2C recovered after %d errors (%u successful reads)\n", 
                   i2c_error_count, i2c_success_count);
        }
        // Decrementar errores gradualmente (más tolerante a errores esporádicos)
        if (i2c_error_count > 0) i2c_error_count--;
        
        // Health check cada 10 segundos: Si hay muchos errores acumulados, reiniciar preventivamente
        if ((now - last_health_check) > 10000) {
            if (i2c_success_count < 100) {  // Menos de 100 lecturas exitosas en 10 seg es bajo
                printf("[Keyboard] I2C health check: Low success rate, preventive restart...\n");
                i2c_deinit(i2c0);
                sleep_ms(50);
                i2c_device_init();
                sleep_ms(50);
            }
            last_health_check = now;
            i2c_success_count = 0;  // Reset contador para próximo período
        }
        
        // Debug: imprimir valor ADC solo cuando cambia significativamente
        static uint16_t last_adc_debug = 0;
        if (abs((int)adc_value - (int)last_adc_debug) > 100) {
            printf("[Keyboard] ADC value: %d\n", adc_value);
            last_adc_debug = adc_value;
        }
        
        // Determinar qué botón está presionado basándose en rangos ADC
        int8_t detected_btn = -1;  // -1 = NONE
        
        if (adc_value >= ADC_RANGE_UP_MIN && adc_value <= ADC_RANGE_UP_MAX) {
            detected_btn = KEY_UP;
        } else if (adc_value >= ADC_RANGE_DOWN_MIN && adc_value <= ADC_RANGE_DOWN_MAX) {
            detected_btn = KEY_DOWN;
        } else if (adc_value >= ADC_RANGE_LEFT_MIN && adc_value <= ADC_RANGE_LEFT_MAX) {
            detected_btn = KEY_LEFT;
        } else if (adc_value >= ADC_RANGE_RIGHT_MIN && adc_value <= ADC_RANGE_RIGHT_MAX) {
            detected_btn = KEY_RIGHT;
        }
        
        // Debounce: requiere 2 lecturas consecutivas iguales
        if (detected_btn == dpad_debounce_btn) {
            if (dpad_debounce_count < 255) dpad_debounce_count++;
        } else {
            dpad_debounce_btn = detected_btn;
            dpad_debounce_count = 1;
        }
        
        // Solo aplicar estado si hay al menos 2 lecturas consecutivas
        bool keyStates[4] = {false, false, false, false};
        if (dpad_debounce_count >= 2 && detected_btn >= 0) {
            keyStates[detected_btn] = true;
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
        
        // Log solo cada ciertos errores para evitar spam
        if (i2c_error_count == 1 || i2c_error_count == 5 || i2c_error_count % 10 == 0) {
            printf("[Keyboard] I2C read failed (error count: %d)\n", i2c_error_count);
        }
        
        // Si falla la lectura I2C, marcar como no presionado
        for (uint8_t i = KEY_UP; i <= KEY_RIGHT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
    }
}
