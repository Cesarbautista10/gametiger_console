#include "keyboard.h"
#include "audio.h"

namespace {

// X/PA1 contiene los cuatro botones de acción. Y/PA0 contiene el D-pad.
// Cada escalera permite una tecla a la vez, pero ambas se leen juntas y por
// eso sí permiten una dirección y una acción simultáneas.
constexpr uint8_t DPAD_KEY_MAP[4] = {
    KEY_UP, KEY_RIGHT, KEY_LEFT, KEY_DOWN
};
constexpr uint8_t ACTION_KEY_MAP[4] = {
    KEY_B, KEY_START, KEY_A, KEY_SELECT
};
constexpr uint16_t DPAD_SLOT_MIN[4] = {
    DPAD_ADC_SLOT_1_MIN, DPAD_ADC_SLOT_2_MIN,
    DPAD_ADC_SLOT_3_MIN, DPAD_ADC_SLOT_4_MIN
};
constexpr uint16_t DPAD_SLOT_MAX[4] = {
    DPAD_ADC_SLOT_1_MAX, DPAD_ADC_SLOT_2_MAX,
    DPAD_ADC_SLOT_3_MAX, DPAD_ADC_SLOT_4_MAX
};
constexpr uint16_t ACTION_SLOT_MIN[4] = {
    ACTION_ADC_SLOT_1_MIN, ACTION_ADC_SLOT_2_MIN,
    ACTION_ADC_SLOT_3_MIN, ACTION_ADC_SLOT_4_MIN
};
constexpr uint16_t ACTION_SLOT_MAX[4] = {
    ACTION_ADC_SLOT_1_MAX, ACTION_ADC_SLOT_2_MAX,
    ACTION_ADC_SLOT_3_MAX, ACTION_ADC_SLOT_4_MAX
};

}  // namespace

KeyBoard::KeyBoard() {
    printf("[Keyboard] joystick driver loading...\n");

    i2c_device_init();
    joystick_i2c_addr = JOYSTICK_I2C_ADDR;
    i2c_enabled = false;
    i2c_error_count = 0;
    last_i2c_check = 0;
    last_i2c_retry = 0;
    joystick_frame_requested_at = 0;
    joystick_frame_pending = false;
    last_x_debug = 0xFFFF;
    last_y_debug = 0xFFFF;

    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        prevKeyState[i] = false;
    }
    resetDebounce();

    // No se inicializan GPIO para A/B/START/SELECT: en esta revisión los
    // ocho botones llegan por las dos escaleras ADC del joystick I2C.
    i2c_enabled = findI2CJoystick();
    if (i2c_enabled) {
        printf("[Keyboard] DDP joystick enabled at address 0x%02X\n",
               static_cast<unsigned>(joystick_i2c_addr));
    } else {
        printf("[Keyboard] DDP joystick not found; retrying every 2 seconds\n");
    }
    printf("[Keyboard] Done\n");
}

KeyBoard::~KeyBoard() {
}

void KeyBoard::checkKeyState(Screen *screen) {
    checkI2CJoystick(screen);
}

bool KeyBoard::validateI2CJoystick(uint8_t address) {
    uint16_t device_id = 0;
    if (!readJoystickDeviceId(address, &device_id) ||
        device_id != JOYSTICK_DEVICE_ID) {
        return false;
    }

    JoystickFrame frame{};
    if (!readJoystickFrame(address, &frame)) {
        return false;
    }

    joystick_i2c_addr = address;
    printf("[Keyboard] Joystick 0x%04X found at 0x%02X (X=%u, Y=%u)\n",
           static_cast<unsigned>(device_id), static_cast<unsigned>(address),
           static_cast<unsigned>(frame.x), static_cast<unsigned>(frame.y));
    return true;
}

bool KeyBoard::findI2CJoystick() {
    // Esta consola usa 0x53. También se prueba la dirección de fábrica y
    // direcciones históricas que pueden seguir guardadas en otra botonera.
    constexpr uint8_t preferred_addresses[] = {
        JOYSTICK_I2C_ADDR, JOYSTICK_FACTORY_I2C_ADDR, 0x1A, 0x56
    };

    for (uint8_t address : preferred_addresses) {
        if (validateI2CJoystick(address)) {
            return true;
        }
    }

    // El firmware acepta cualquier dirección persistida entre 0x08 y 0x77.
    // Se exige Device ID 0x0101 para no confundir otro periférico del bus.
    for (uint8_t address = 0x08; address < 0x78; ++address) {
        bool already_checked = false;
        for (uint8_t preferred : preferred_addresses) {
            if (address == preferred) {
                already_checked = true;
                break;
            }
        }
        if (!already_checked && validateI2CJoystick(address)) {
            return true;
        }
    }

    return false;
}

int8_t KeyBoard::decodeLadder(uint16_t adc_value, const uint8_t key_map[4],
                              const uint16_t slot_min[4],
                              const uint16_t slot_max[4]) const {
    for (uint8_t slot = 0; slot < 4; ++slot) {
        if (adc_value >= slot_min[slot] && adc_value <= slot_max[slot]) {
            return key_map[slot];
        }
    }
    return -1;
}

void KeyBoard::updateButtonBank(Screen *screen, int8_t detected_key,
                                int8_t &candidate_key,
                                uint8_t &candidate_count,
                                int8_t &stable_key, uint16_t adc_value,
                                const char *bank_name) {
    if (detected_key == candidate_key) {
        if (candidate_count < JOYSTICK_DEBOUNCE_READS) {
            ++candidate_count;
        }
    } else {
        candidate_key = detected_key;
        candidate_count = 1;
    }

    // Se filtran tanto la pulsación como la liberación. Mientras llega la
    // segunda muestra, la tecla estable anterior continúa presionada.
    if (candidate_count < JOYSTICK_DEBOUNCE_READS) {
        if (stable_key >= 0) {
            screen->keyDown(static_cast<uint8_t>(stable_key));
        }
        return;
    }

    if (candidate_key != stable_key) {
        if (stable_key >= 0) {
            const uint8_t old_key = static_cast<uint8_t>(stable_key);
            screen->keyReleased(old_key);
            prevKeyState[old_key] = false;
        }

        stable_key = candidate_key;
        if (stable_key >= 0) {
            const uint8_t new_key = static_cast<uint8_t>(stable_key);
            printf("[Keyboard] %s pressed: key=%u ADC=%u\n", bank_name,
                   static_cast<unsigned>(new_key),
                   static_cast<unsigned>(adc_value));
            screen->keyPressed(new_key);
            prevKeyState[new_key] = true;
            if (globalAudio) {
                globalAudio->playSelectSound();
            }
        }
    } else if (stable_key >= 0) {
        screen->keyDown(static_cast<uint8_t>(stable_key));
    }
}

void KeyBoard::releaseAllKeys(Screen *screen) {
    for (uint8_t key = 0; key < KEY_COUNT; ++key) {
        if (prevKeyState[key]) {
            screen->keyReleased(key);
            prevKeyState[key] = false;
        }
    }
    resetDebounce();
}

void KeyBoard::resetDebounce() {
    x_candidate_key = -1;
    y_candidate_key = -1;
    x_stable_key = -1;
    y_stable_key = -1;
    x_candidate_count = 0;
    y_candidate_count = 0;
}

void KeyBoard::checkI2CJoystick(Screen *screen) {
    const uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!i2c_enabled) {
        if (now - last_i2c_retry >= JOYSTICK_RETRY_INTERVAL_MS) {
            printf("[Keyboard] Searching for DDP joystick...\n");
            i2c_deinit(I2C_PORT);
            sleep_ms(20);
            i2c_device_init();

            if (findI2CJoystick()) {
                i2c_enabled = true;
                i2c_error_count = 0;
                joystick_frame_pending = false;
                printf("[Keyboard] Joystick reconnected at 0x%02X\n",
                       static_cast<unsigned>(joystick_i2c_addr));
            } else {
                printf("[Keyboard] Joystick not found; will retry\n");
            }
            // Medir el intervalo desde que terminó el escaneo. Si el bus
            // tarda en responder, no iniciar otro escaneo inmediatamente.
            last_i2c_retry = to_ms_since_boot(get_absolute_time());
            last_i2c_check = last_i2c_retry;
        }
        return;
    }

    JoystickFrame frame{};
    bool frame_failed = false;

    if (joystick_frame_pending) {
        if (now - joystick_frame_requested_at <
            JOYSTICK_RESPONSE_DELAY_MS) {
            return;
        }
        joystick_frame_pending = false;
        frame_failed = !receiveJoystickFrame(joystick_i2c_addr, &frame);
    } else {
        if (now - last_i2c_check < JOYSTICK_POLL_INTERVAL_MS) {
            return;
        }
        last_i2c_check = now;
        if (requestJoystickFrame(joystick_i2c_addr)) {
            joystick_frame_requested_at = now;
            joystick_frame_pending = true;
            return;
        }
        frame_failed = true;
    }

    if (frame_failed) {
        if (i2c_error_count < 255) {
            ++i2c_error_count;
        }
        if (i2c_error_count == 1 || i2c_error_count == 5 ||
            i2c_error_count == JOYSTICK_MAX_ERRORS) {
            printf("[Keyboard] Joystick frame failed (%u/%u)\n",
                   static_cast<unsigned>(i2c_error_count),
                   static_cast<unsigned>(JOYSTICK_MAX_ERRORS));
        }

        // No dejar una tecla pegada si el cable se desconecta. Un fallo
        // aislado no altera el estado; tres fallos consecutivos sí liberan.
        if (i2c_error_count == 3) {
            releaseAllKeys(screen);
        }
        if (i2c_error_count >= JOYSTICK_MAX_ERRORS) {
            i2c_enabled = false;
            joystick_frame_pending = false;
            last_i2c_retry = now;
            releaseAllKeys(screen);
            printf("[Keyboard] Joystick offline; retrying in 2 seconds\n");
        }
        return;
    }

    if (i2c_error_count > 0) {
        printf("[Keyboard] I2C recovered after %u consecutive errors\n",
               static_cast<unsigned>(i2c_error_count));
    }
    i2c_error_count = 0;

    // Estos valores permiten ajustar por separado las ventanas de X e Y
    // usando el monitor USB, sin inundarlo con una línea en cada frame.
    if (last_x_debug == 0xFFFF ||
        abs(static_cast<int>(frame.x) - static_cast<int>(last_x_debug)) > 100 ||
        last_y_debug == 0xFFFF ||
        abs(static_cast<int>(frame.y) - static_cast<int>(last_y_debug)) > 100) {
        printf("[Keyboard] Joystick X=%u Y=%u\n",
               static_cast<unsigned>(frame.x),
               static_cast<unsigned>(frame.y));
        last_x_debug = frame.x;
        last_y_debug = frame.y;
    }
    const int8_t dpad_key = decodeLadder(frame.y, DPAD_KEY_MAP,
                                         DPAD_SLOT_MIN, DPAD_SLOT_MAX);
    const int8_t action_key = decodeLadder(frame.x, ACTION_KEY_MAP,
                                           ACTION_SLOT_MIN, ACTION_SLOT_MAX);
    updateButtonBank(screen, dpad_key, y_candidate_key, y_candidate_count,
                     y_stable_key, frame.y, "D-pad");
    updateButtonBank(screen, action_key, x_candidate_key, x_candidate_count,
                     x_stable_key, frame.x, "Action");
}
