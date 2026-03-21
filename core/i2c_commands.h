// ═══════════════════════════════════════════════════════════
// I2C_COMMANDS.H - Definiciones de comandos I2C
// ═══════════════════════════════════════════════════════════

#ifndef I2C_COMMANDS_H
#define I2C_COMMANDS_H

// ═══════════════════════════════════════════════════════════
//           COMANDOS CRÍTICOS DE SEGURIDAD (0xA0-0xAF)
// ═══════════════════════════════════════════════════════════
#define CMD_RELAY_OFF      0xA0   // Apagar relé PA2 y PB5
#define CMD_RELAY_ON       0xA1   // Encender relé PA2 y PB5
#define CMD_RELAY_TOGGLE   0xA6   // Pulso/Disparo relé PA2 y PB5

// ═══════════════════════════════════════════════════════════
//           COMANDOS NEOPIXEL
// ═══════════════════════════════════════════════════════════
#define CMD_RED            0x02   // NeoPixels rojos
#define CMD_GREEN          0x03   // NeoPixels verdes
#define CMD_BLUE           0x04   // NeoPixels azules
#define CMD_OFF            0x05   // NeoPixels apagados + PWM OFF
#define CMD_WHITE          0x08   // NeoPixels blancos

// ═══════════════════════════════════════════════════════════
//           COMANDOS PWM / BUZZER
// ═══════════════════════════════════════════════════════════
#define CMD_PWM_OFF        0x20   // PWM OFF - Silencio
#define CMD_PWM_25         0x21   // PWM 25% - Tono grave 200Hz
#define CMD_PWM_50         0x22   // PWM 50% - Tono medio 500Hz
#define CMD_PWM_75         0x23   // PWM 75% - Tono agudo 1000Hz
#define CMD_PWM_100        0x24   // PWM 100% - Tono muy agudo 2000Hz

// ═══════════════════════════════════════════════════════════
//           COMANDOS LECTURA DIGITAL
// ═══════════════════════════════════════════════════════════
#define CMD_PA4_DIGITAL    0x07   // Leer PA0 como entrada digital
#define RESP_PA4_DIGITAL   0x09   // Respuesta PA4 digital

// ═══════════════════════════════════════════════════════════
//           COMANDOS ADC (12-bit)
// ═══════════════════════════════════════════════════════════
// PA0 ADC Commands
#define CMD_ADC_PA0_HSB    0x56   // Leer ADC PA0 - HSB (bits 11-8)
#define CMD_ADC_PA0_LSB    0x57   // Leer ADC PA0 - LSB (bits 7-0)

// PA1 ADC Commands
#define CMD_ADC_PA1_HSB    0xD8   // Leer ADC PA1 - HSB (bits 11-8)
#define CMD_ADC_PA1_LSB    0xD9   // Leer ADC PA1 - LSB (bits 7-0)

// Códigos de respuesta ADC
#define RESP_ADC_HSB       0x0D   // ADC HSB enviado (bits 11-8)
#define RESP_ADC_LSB       0x0E   // ADC LSB enviado (bits 7-0)

// ═══════════════════════════════════════════════════════════
//           COMANDOS I2C ADDRESS MANAGEMENT
// ═══════════════════════════════════════════════════════════
#define CMD_SET_I2C_ADDR   0x3D   // Establecer nueva dirección I2C (se guarda en Flash)
#define CMD_RESET_FACTORY  0x3E   // Reset factory (usar UID por defecto)
#define CMD_GET_I2C_STATUS 0x3F   // Obtener estado I2C (Flash vs UID)

#define RESP_I2C_ADDR_SET  0x0D   // Nueva dirección I2C establecida
#define RESP_FACTORY_RESET 0x0E   // Reset factory ejecutado (usar UID)
#define RESP_I2C_FROM_FLASH 0x0F  // I2C usando dirección desde Flash
#define RESP_I2C_FROM_UID  0x0A   // I2C usando dirección desde UID

#endif // I2C_COMMANDS_H
