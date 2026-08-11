// ═══════════════════════════════════════════════════════════
// I2C_DEVICE.H - Funciones de comunicación I2C con dispositivos
// ═══════════════════════════════════════════════════════════

#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

#include "common.h"
#include "i2c_commands.h"

// Configuración I2C para Pico SDK
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define I2C_FREQ 100000  // 100kHz

// Variables globales para tracking de dispositivos
extern uint8_t found_devices[128];
extern uint8_t device_count;
extern uint32_t i2c_ready_time;

// Estructuras
struct DeviceError {
  uint8_t address;
  uint32_t error_count;
  uint32_t success_count;
  uint32_t last_error_time;
};

struct DigitalReadStats {
  uint8_t address;
  uint32_t read_count;
  uint32_t fail_count;
  uint8_t last_state;
  uint32_t last_read_time;
};

// Trama atómica del firmware unit_firmware_i2c_joystick_py32.
// X proviene de PA1/ADC_IN1 y Y de PA0/ADC_IN0. El bit de switch se conserva
// en el protocolo, pero esta variante lo devuelve siempre liberado.
struct JoystickFrame {
  uint16_t x;
  uint16_t y;
  bool switch_pressed;
};

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE COMUNICACIÓN I2C BÁSICA
// ═══════════════════════════════════════════════════════════

// Enviar comando simple (1 byte) a dispositivo I2C
bool sendCommand(uint8_t address, uint8_t cmd);

// Enviar comando y leer respuesta (1 byte)
bool sendCommandAndRead(uint8_t address, uint8_t cmd, uint8_t* response);

// Identificar y leer el joystick DDP sin usar los comandos ADC heredados.
bool readJoystickDeviceId(uint8_t address, uint16_t* device_id);
bool readJoystickFrame(uint8_t address, JoystickFrame* frame);
// Versión dividida para el bucle principal: permite esperar los 10 ms que
// necesita el esclavo sin detener la emulación de Game Boy.
bool requestJoystickFrame(uint8_t address);
bool receiveJoystickFrame(uint8_t address, JoystickFrame* frame);

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL DE RELAY
// ═══════════════════════════════════════════════════════════

bool relayToggle(uint8_t address);
bool relayOn(uint8_t address);
bool relayOff(uint8_t address);

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL NEOPIXEL
// ═══════════════════════════════════════════════════════════

bool neoRed(uint8_t address);
bool neoGreen(uint8_t address);
bool neoBlue(uint8_t address);
bool neoWhite(uint8_t address);
bool neoOff(uint8_t address);

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL PWM
// ═══════════════════════════════════════════════════════════

bool pwmOff(uint8_t address);
bool pwm25(uint8_t address);
bool pwm50(uint8_t address);
bool pwm75(uint8_t address);
bool pwm100(uint8_t address);

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE LECTURA DIGITAL/ADC
// ═══════════════════════════════════════════════════════════

bool readPA0Digital(uint8_t address, uint8_t* state);
bool readADC_HSB(uint8_t address, uint8_t* hsb);
bool readADC_LSB(uint8_t address, uint8_t* lsb);
bool readADC_Full(uint8_t address, uint16_t* value);

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE GESTIÓN DE DIRECCIÓN I2C
// ═══════════════════════════════════════════════════════════

bool setI2CAddress(uint8_t current_addr, uint8_t new_addr);
bool factoryReset(uint8_t address);
bool getI2CStatus(uint8_t address, uint8_t* status);

// Inicialización I2C
void i2c_device_init();

#endif // I2C_DEVICE_H
