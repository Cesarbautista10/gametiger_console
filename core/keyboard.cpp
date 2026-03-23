#include "keyboard.h"
#include "audio.h"

// Mapeo de botones ADC1 (PA1): posición en rango → KEY
// BTN1(0-500)=START, BTN2(600-1500)=B, BTN3(1550-2350)=A, BTN4(2400-3300)=SELECT
static const uint8_t action_button_map[4] = {KEY_START, KEY_B, KEY_A, KEY_SELECT};
// Mapeo de botones ADC0 (PA0): posición en rango → KEY
static const uint8_t dpad_button_map[4] = {KEY_UP, KEY_RIGHT, KEY_LEFT, KEY_DOWN};

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
    action_debounce_btn = -1;
    action_debounce_count = 0;
    read_toggle = false;
    
    // Inicializar estados previos
    for (uint8_t i = 0; i < KEY_COUNT; i++)
        this->prevKeyState[i] = false;

    // No GPIO init - todos los botones son vía ADC/I2C
    
    printf("[Keyboard] I2C ADC buttons at address 0x%02X\n", DPAD_I2C_ADDR);
    printf("[Keyboard] ADC0(PA0)=D-Pad, ADC1(PA1)=A,B,START,SELECT\n");
    printf("[Keyboard] Done\n");
}

KeyBoard::~KeyBoard() {
}

void KeyBoard::checkKeyState(Screen *screen) {
    if (!i2c_enabled) {
        // Intentar reconectar (se hace dentro de checkI2CDPad)
        checkI2CDPad(screen);
        return;
    }
    
    // Alternar lectura ADC0/ADC1 cada ciclo para reducir carga I2C
    if (read_toggle) {
        checkI2CDPad(screen);
    } else {
        checkI2CActionButtons(screen);
    }
    read_toggle = !read_toggle;
}

// Decodificar valor ADC a índice de botón usando rangos compartidos
// Retorna el KEY correspondiente, o -1 si ningún botón presionado
int8_t KeyBoard::decodeADCButton(uint16_t adc_value, const uint8_t* button_map) {
    if (adc_value >= ADC_RANGE_UP_MIN && adc_value <= ADC_RANGE_UP_MAX) {
        return button_map[0];  // BTN1
    } else if (adc_value >= ADC_RANGE_RIGHT_MIN && adc_value <= ADC_RANGE_RIGHT_MAX) {
        return button_map[1];  // BTN2
    } else if (adc_value >= ADC_RANGE_LEFT_MIN && adc_value <= ADC_RANGE_LEFT_MAX) {
        return button_map[2];  // BTN3
    } else if (adc_value >= ADC_RANGE_DOWN_MIN && adc_value <= ADC_RANGE_DOWN_MAX) {
        return button_map[3];  // BTN4
    }
    return -1;  // NONE (3500-4095 o gaps entre rangos)
}

void KeyBoard::checkI2CDPad(Screen *screen) {
    // Usar siempre el mismo sistema de tiempo
    uint32_t now = time_us_32() / 1000;
    
    // Si I2C está deshabilitado, intentar reconectar cada 2 segundos
    if (!i2c_enabled) {
        uint32_t elapsed = now - last_i2c_retry;
        if (elapsed > 2000) {
            printf("[Keyboard] Attempting I2C reconnection (elapsed: %u ms)...\n", elapsed);
            
            i2c_deinit(i2c0);
            sleep_ms(100);
            i2c_device_init();
            sleep_ms(100);
            
            printf("[Keyboard] Scanning I2C bus for device 0x%02X...\n", DPAD_I2C_ADDR);
            bool device_found = false;
            
            int scan_result = i2c_write_blocking(i2c0, DPAD_I2C_ADDR, NULL, 0, false);
            if (scan_result >= 0) {
                device_found = true;
            } else {
                uint8_t dummy = 0;
                scan_result = i2c_write_blocking(i2c0, DPAD_I2C_ADDR, &dummy, 1, false);
                if (scan_result >= 0) device_found = true;
            }
            
            if (device_found) {
                sleep_ms(50);
                uint16_t test_value = 0;
                if (readADC_Full(DPAD_I2C_ADDR, &test_value)) {
                    printf("[Keyboard] I2C reconnected! ADC test: %d\n", test_value);
                    i2c_error_count = 0;
                    i2c_enabled = true;
                    last_i2c_check = now;
                }
            }
            
            last_i2c_retry = now;
        }
        return;
    }
    
    // Solo verificar cada 30ms para reducir carga I2C
    if (now - last_i2c_check < 30) {
        return;
    }
    last_i2c_check = now;
    
    // Si hubo muchos errores consecutivos, desactivar temporalmente
    if (i2c_error_count > 30) {
        if (i2c_enabled) {
            printf("[Keyboard] I2C disabled due to errors (count: %d)\n", i2c_error_count);
            i2c_enabled = false;
            last_i2c_retry = now;
        }
        // Liberar todos los botones
        for (uint8_t i = 0; i < KEY_COUNT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
        return;
    }
    
    uint16_t adc_value = 0;
    
    if (readADC_Full(DPAD_I2C_ADDR, &adc_value)) {
        i2c_success_count++;
        
        if (i2c_error_count > 3) {
            printf("[Keyboard] I2C recovered after %d errors\n", i2c_error_count);
        }
        if (i2c_error_count > 0) i2c_error_count--;
        
        // Decodificar botón usando función compartida
        int8_t detected_btn = decodeADCButton(adc_value, dpad_button_map);
        
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
        
        if (i2c_error_count == 1 || i2c_error_count == 5 || i2c_error_count % 10 == 0) {
            printf("[Keyboard] I2C ADC0 read failed (error count: %d)\n", i2c_error_count);
        }
        
        // Si falla, liberar botones del D-Pad
        for (uint8_t i = KEY_UP; i <= KEY_RIGHT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
    }
}

void KeyBoard::checkI2CActionButtons(Screen *screen) {
    if (!i2c_enabled) return;
    
    uint32_t now = time_us_32() / 1000;
    
    // Compartir el throttle con checkI2CDPad (ya se verificó en checkKeyState)
    
    uint16_t adc_value = 0;
    
    if (readADC1_Full(DPAD_I2C_ADDR, &adc_value)) {
        // Decodificar botón usando función compartida con mapeo de acción
        int8_t detected_btn = decodeADCButton(adc_value, action_button_map);
        
        // Debounce: requiere 2 lecturas consecutivas iguales
        if (detected_btn == action_debounce_btn) {
            if (action_debounce_count < 255) action_debounce_count++;
        } else {
            action_debounce_btn = detected_btn;
            action_debounce_count = 1;
        }
        
        // Solo aplicar estado si hay al menos 2 lecturas consecutivas
        bool keyStates[KEY_COUNT] = {false};
        if (action_debounce_count >= 2 && detected_btn >= 0) {
            keyStates[detected_btn] = true;
        }
        
        // Procesar cambios de estado para botones de acción
        for (uint8_t i = KEY_A; i < KEY_COUNT; i++) {
            bool keyState = keyStates[i];
            
            if (this->prevKeyState[i] != keyState) {
                if (keyState) {
                    printf("[Keyboard] ACTION pressed: %d (ADC1: %d)\n", i, adc_value);
                    screen->keyPressed(i);
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
        
        if (i2c_error_count == 1 || i2c_error_count == 5 || i2c_error_count % 10 == 0) {
            printf("[Keyboard] I2C ADC1 read failed (error count: %d)\n", i2c_error_count);
        }
        
        // Si falla, liberar botones de acción
        for (uint8_t i = KEY_A; i < KEY_COUNT; i++) {
            if (this->prevKeyState[i]) {
                screen->keyReleased(i);
                this->prevKeyState[i] = false;
            }
        }
    }
}
