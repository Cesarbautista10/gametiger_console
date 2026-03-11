// ═══════════════════════════════════════════════════════════
// I2C_SCANNER.H - Funciones de escaneo y detección de dispositivos I2C
// ═══════════════════════════════════════════════════════════

#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include <Arduino.h>
#include <Wire.h>

// Configuración I2C externa
extern TwoWire& WIRE;
extern uint8_t found_devices[128];
extern uint8_t device_count;
extern uint32_t i2c_ready_time;

// Constantes
#define DEVICE_DELAY 50  // Delay entre dispositivos (ms)

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE ESCANEO Y UTILIDADES I2C
// ═══════════════════════════════════════════════════════════

// Escanear bus I2C y mostrar dispositivos encontrados
void scanI2CBus();

// Verificar si dispositivo existe en dirección
bool deviceExists(uint8_t address);

// Parsear string hexadecimal a dirección I2C
uint8_t parseHexAddress(String hex_str);

// Convertir dirección a string hexadecimal
String addressToHexString(uint8_t address);

#endif // I2C_SCANNER_H
