// ═══════════════════════════════════════════════════════════
// I2C_SCANNER.CPP - Implementación de funciones de escaneo
// ═══════════════════════════════════════════════════════════

#include "i2c_scanner.h"

// ═══════════════════════════════════════════════════════════
//           FUNCIÓN DE ESCANEO I2C
// ═══════════════════════════════════════════════════════════

void scanI2CBus() {
  // Verificar que haya pasado suficiente tiempo desde init
  if (millis() - i2c_ready_time < 500) {
    uint32_t wait_time = 500 - (millis() - i2c_ready_time);
    delay(wait_time);
  }
  
  device_count = 0;
  
  for (uint8_t address = 0x08; address < 0x78; address++) {
    WIRE.beginTransmission(address);
    uint8_t error = WIRE.endTransmission();
    
    if (error == 0) {
      found_devices[device_count++] = address;
      Serial.print("0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
    
    delay(DEVICE_DELAY);
  }
}

// ═══════════════════════════════════════════════════════════
//           VERIFICAR SI DIRECCIÓN EXISTE
// ═══════════════════════════════════════════════════════════

bool deviceExists(uint8_t address) {
  WIRE.beginTransmission(address);
  return (WIRE.endTransmission() == 0);
}

// ═══════════════════════════════════════════════════════════
//           PARSEO DE DIRECCIÓN HEXADECIMAL
// ═══════════════════════════════════════════════════════════

uint8_t parseHexAddress(String hex_str) {
  // Remover espacios
  hex_str.trim();
  
  // Convertir de hex a int
  char* endptr;
  long addr = strtol(hex_str.c_str(), &endptr, 16);
  
  // Validar rango
  if (addr >= 0x08 && addr <= 0x77) {
    return (uint8_t)addr;
  }
  
  return 0; // Dirección inválida
}

// ═══════════════════════════════════════════════════════════
//           CONVERTIR DIRECCIÓN A FORMATO HEX STRING
// ═══════════════════════════════════════════════════════════

String addressToHexString(uint8_t address) {
  String result = "0x";
  if (address < 16) result += "0";
  result += String(address, HEX);
  result.toUpperCase();
  return result;
}
