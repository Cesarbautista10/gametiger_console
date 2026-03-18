#include "core/common.h"

int main() {
    set_sys_clock_khz(266 * 1000, true);
    sleep_ms(50);
    stdio_init_all();
    sleep_ms(1000);

    printf("\n[I2C Scanner] Starting...\n");
    
    // Inicializar I2C a 100kHz como requiere el dispositivo
    i2c_init(i2c0, 100000);
    gpio_set_function(24, GPIO_FUNC_I2C);
    gpio_set_function(25, GPIO_FUNC_I2C);
    gpio_pull_up(24);
    gpio_pull_up(25);
    
    printf("[I2C Scanner] I2C0 initialized on SDA=24, SCL=25 @ 100kHz\n\n");
    
    while (true) {
        printf("━━━ SCANNING I2C BUS ━━━\n");
        int devices_found = 0;
        
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            // Método correcto: enviar solo la dirección y esperar ACK/NACK
            uint8_t txdata = 0;
            int ret = i2c_write_blocking(i2c0, addr, &txdata, 1, false);
            
            // Verificar si hubo ACK (ret debe ser >= 0)
            // Si el dispositivo no existe, retorna PICO_ERROR_GENERIC (-1)
            if (ret >= 0) {
                printf("  [%02d] 0x%02X (%3d) - FOUND\n", devices_found + 1, addr, addr);
                devices_found++;
            }
        }
        
        if (devices_found == 0) {
            printf("  No devices found\n");
        }
        printf("━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Total: %d device(s)\n\n", devices_found);
        
        sleep_ms(2000);
    }
    
    return 0;
}
