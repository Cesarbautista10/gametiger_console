#include "core/common.h"

#define DEVICE_ADDR 0x56

int main() {
    set_sys_clock_khz(266 * 1000, true);
    sleep_ms(50);
    stdio_init_all();
    sleep_ms(1000);

    printf("\n[I2C Test] Starting...\n");
    
    // Inicializar I2C a 100kHz (como Arduino)
    i2c_init(i2c0, 100000);
    gpio_set_function(24, GPIO_FUNC_I2C);
    gpio_set_function(25, GPIO_FUNC_I2C);
    gpio_pull_up(24);
    gpio_pull_up(25);
    
    printf("[I2C Test] I2C initialized @ 100kHz\n\n");
    
    while (true) {
        // Primero: I2C Scan
        printf("━━━ I2C Scan ━━━\n");
        bool found = false;
        for (int addr = 0x08; addr < 0x78; addr++) {
            uint8_t dummy = 0;
            int ret = i2c_write_blocking(i2c0, addr, &dummy, 1, false);
            if (ret >= 0) {
                printf("Device found at 0x%02X\n", addr);
                if (addr == DEVICE_ADDR) found = true;
            }
        }
        if (!found) {
            printf("Device 0x56 NOT FOUND!\n\n");
            sleep_ms(2000);
            continue;
        }
        printf("\n");
        
        // Prueba 1: Lectura directa (sin comando) - SKIP (causa timeout)
        printf("━━━ Test 1: SKIPPED (direct read not supported) ━━━\n\n");
        
        // NO reset I2C - mantener bus limpio
        
        // Prueba 2: Comando 0xD8 + STOP + delay + lectura - BLOCKING
        printf("━━━ Test 2: CMD 0xD8 + STOP + delay + read ━━━\n");
        printf("About to write 0xD8...\n");
        uint8_t cmd_hsb = 0xD8;
        int ret2a = i2c_write_blocking(i2c0, DEVICE_ADDR, &cmd_hsb, 1, false); // STOP - igual que scan
        printf("Write result: %d\n", ret2a);
        if (ret2a == 1) {
            sleep_ms(10);  // delay como Arduino
            printf("About to read response...\n");
            uint8_t hsb;
            absolute_time_t timeout = make_timeout_time_ms(100);
            int ret2b = i2c_read_blocking_until(i2c0, DEVICE_ADDR, &hsb, 1, false, timeout);
            printf("Read result: %d\n", ret2b);
            if (ret2b == 1) {
                printf("SUCCESS: HSB=0x%02X\n", hsb);
            } else if (ret2b == PICO_ERROR_TIMEOUT) {
                printf("TIMEOUT on read\n");
            } else {
                printf("FAILED on read: ret=%d\n", ret2b);
            }
        } else if (ret2a == PICO_ERROR_TIMEOUT) {
            printf("TIMEOUT on write\n");
        } else {
            printf("FAILED on write: ret=%d\n", ret2a);
        }
        
        sleep_ms(100);
        
        // Prueba 3: Comando 0xD9 + STOP + delay + lectura - BLOCKING
        printf("━━━ Test 3: CMD 0xD9 + STOP + delay + read ━━━\n");
        printf("About to write 0xD9...\n");
        uint8_t cmd_lsb = 0xD9;
        int ret3a = i2c_write_blocking(i2c0, DEVICE_ADDR, &cmd_lsb, 1, false); // STOP - igual que scan
        printf("Write result: %d\n", ret3a);
        if (ret3a == 1) {
            sleep_ms(10);  // delay como Arduino
            uint8_t lsb;
            absolute_time_t timeout = make_timeout_time_ms(100);
            int ret3b = i2c_read_blocking_until(i2c0, DEVICE_ADDR, &lsb, 1, false, timeout);
            if (ret3b == 1) {
                printf("SUCCESS: LSB=0x%02X\n", lsb);
            } else if (ret3b == PICO_ERROR_TIMEOUT) {
                printf("TIMEOUT on read\n");
            } else {
                printf("FAILED on read: ret=%d\n", ret3b);
            }
        } else {
            printf("FAILED on write: ret=%d\n", ret3a);
        }
        
        sleep_ms(100);
        
        // Prueba 4: Full ADC (0xD8 + 0xD9) - BLOCKING
        printf("━━━ Test 4: Full ADC (0xD8 + 0xD9) ━━━\n");
        uint8_t hsb_val, lsb_val;
        bool success = false;
        
        cmd_hsb = 0xD8;
        if (i2c_write_blocking(i2c0, DEVICE_ADDR, &cmd_hsb, 1, false) == 1) {
            sleep_ms(10);
            absolute_time_t timeout4 = make_timeout_time_ms(100);
            if (i2c_read_blocking_until(i2c0, DEVICE_ADDR, &hsb_val, 1, false, timeout4) == 1) {
                sleep_ms(10);
                cmd_lsb = 0xD9;
                if (i2c_write_blocking(i2c0, DEVICE_ADDR, &cmd_lsb, 1, false) == 1) {
                    sleep_ms(10);
                    timeout4 = make_timeout_time_ms(100);
                    if (i2c_read_blocking_until(i2c0, DEVICE_ADDR, &lsb_val, 1, false, timeout4) == 1) {
                        uint16_t adc = ((uint16_t)hsb_val << 8) | lsb_val;
                        printf("SUCCESS: %4d (%.3fV) [HSB=0x%02X LSB=0x%02X]\n", 
                               adc, (adc * 3.3) / 4095.0, hsb_val, lsb_val);
                        success = true;
                    }
                }
            }
        }
        if (!success) printf("FAILED\n");
        
        printf("\n");
        sleep_ms(2000);
    }
    
    return 0;
}
