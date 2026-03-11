// ═══════════════════════════════════════════════════════════
// I2C_DEVICE.CPP - Implementación de funciones I2C
// ═══════════════════════════════════════════════════════════

#include "i2c_device.h"

// ═══════════════════════════════════════════════════════════
//           INICIALIZACIÓN I2C
// ═══════════════════════════════════════════════════════════

void i2c_device_init() {
  i2c_init(I2C_PORT, I2C_FREQ);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);
  printf("[I2C Device] Initialized on SDA=%d, SCL=%d @ %dHz\n", I2C_SDA, I2C_SCL, I2C_FREQ);
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE COMUNICACIÓN I2C BÁSICA
// ═══════════════════════════════════════════════════════════

bool sendCommand(uint8_t address, uint8_t cmd) {
  int result = i2c_write_blocking_until(I2C_PORT, address, &cmd, 1, false, make_timeout_time_ms(10));
  return (result == 1);
}

bool sendCommandAndRead(uint8_t address, uint8_t cmd, uint8_t* response) {
  // Enviar comando con STOP y timeout
  absolute_time_t timeout = make_timeout_time_ms(5);
  int ret_write = i2c_write_blocking_until(I2C_PORT, address, &cmd, 1, false, timeout);
  if (ret_write == 1) {
    sleep_ms(10);  // Delay para que slave procese (como Arduino)
    
    // Leer respuesta con timeout
    timeout = make_timeout_time_ms(5);
    int ret_read = i2c_read_blocking_until(I2C_PORT, address, response, 1, false, timeout);
    return (ret_read == 1);
  }
  return false;
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL DE RELAY
// ═══════════════════════════════════════════════════════════

bool relayToggle(uint8_t address) {
  return sendCommand(address, CMD_RELAY_TOGGLE);
}

bool relayOn(uint8_t address) {
  return sendCommand(address, CMD_RELAY_ON);
}

bool relayOff(uint8_t address) {
  return sendCommand(address, CMD_RELAY_OFF);
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL NEOPIXEL
// ═══════════════════════════════════════════════════════════

bool neoRed(uint8_t address) {
  return sendCommand(address, CMD_RED);
}

bool neoGreen(uint8_t address) {
  return sendCommand(address, CMD_GREEN);
}

bool neoBlue(uint8_t address) {
  return sendCommand(address, CMD_BLUE);
}

bool neoWhite(uint8_t address) {
  return sendCommand(address, CMD_WHITE);
}

bool neoOff(uint8_t address) {
  return sendCommand(address, CMD_OFF);
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE CONTROL PWM
// ═══════════════════════════════════════════════════════════

bool pwmOff(uint8_t address) {
  return sendCommand(address, CMD_PWM_OFF);
}

bool pwm25(uint8_t address) {
  return sendCommand(address, CMD_PWM_25);
}

bool pwm50(uint8_t address) {
  return sendCommand(address, CMD_PWM_50);
}

bool pwm75(uint8_t address) {
  return sendCommand(address, CMD_PWM_75);
}

bool pwm100(uint8_t address) {
  return sendCommand(address, CMD_PWM_100);
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE LECTURA DIGITAL/ADC
// ═══════════════════════════════════════════════════════════

bool readPA0Digital(uint8_t address, uint8_t* state) {
  uint8_t response;
  if (sendCommandAndRead(address, CMD_PA4_DIGITAL, &response)) {
    // Extraer el bit PA0 del byte de respuesta
    *state = (response & 0x01);
    return true;
  }
  return false;
}

bool readADC_HSB(uint8_t address, uint8_t* hsb) {
  return sendCommandAndRead(address, CMD_ADC_PA0_HSB, hsb);
}

bool readADC_LSB(uint8_t address, uint8_t* lsb) {
  return sendCommandAndRead(address, CMD_ADC_PA0_LSB, lsb);
}

bool readADC_I2C(uint8_t address, uint8_t* scaled) {
  return sendCommandAndRead(address, CMD_ADC_PA0_I2C, scaled);
}

bool readADC_Full(uint8_t address, uint16_t* value) {
  // Lectura usando comandos HSB y LSB separados
  uint8_t hsb, lsb;
  if (readADC_HSB(address, &hsb) && readADC_LSB(address, &lsb)) {
    // Combinar bytes y aplicar máscara de 12 bits (ADC de 12 bits = 0-4095)
    *value = (((uint16_t)hsb << 8) | lsb) & 0x0FFF;
    return true;
  }
  return false;
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE GESTIÓN DE DIRECCIÓN I2C
// ═══════════════════════════════════════════════════════════

bool setI2CAddress(uint8_t current_addr, uint8_t new_addr) {
  uint8_t data[2] = {CMD_SET_I2C_ADDR, new_addr};
  int result = i2c_write_blocking_until(I2C_PORT, current_addr, data, 2, false, make_timeout_time_ms(10));
  return (result == 2);
}

bool factoryReset(uint8_t address) {
  return sendCommand(address, CMD_RESET_FACTORY);
}

bool getI2CStatus(uint8_t address, uint8_t* status) {
  return sendCommandAndRead(address, CMD_GET_I2C_STATUS, status);
}
