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
  int result = i2c_write_blocking_until(I2C_PORT, address, &cmd, 1, false, make_timeout_time_ms(50));
  return (result == 1);
}

static bool sendCommandAndReadBytes(uint8_t address, uint8_t cmd,
                                    uint8_t* response, size_t response_length,
                                    uint32_t processing_delay_ms) {
  if (response == nullptr || response_length == 0) {
    return false;
  }

  // Enviar comando con STOP
  absolute_time_t timeout = make_timeout_time_ms(50);
  int ret_write = i2c_write_blocking_until(I2C_PORT, address, &cmd, 1, false, timeout);
  if (ret_write == 1) {
    sleep_ms(processing_delay_ms);
    
    // Leer respuesta
    timeout = make_timeout_time_ms(50);
    int ret_read = i2c_read_blocking_until(I2C_PORT, address, response,
                                           response_length, false, timeout);
    return ret_read == static_cast<int>(response_length);
  }
  return false;
}

bool sendCommandAndRead(uint8_t address, uint8_t cmd, uint8_t* response) {
  return sendCommandAndReadBytes(address, cmd, response, 1, 10);
}

bool readJoystickDeviceId(uint8_t address, uint16_t* device_id) {
  if (device_id == nullptr) {
    return false;
  }

  uint8_t response[2];
  if (!sendCommandAndReadBytes(address, CMD_GET_DEVICE_ID, response,
                               sizeof(response), 5)) {
    return false;
  }

  // DDP transmite enteros multibyte little-endian.
  *device_id = static_cast<uint16_t>(response[0]) |
               (static_cast<uint16_t>(response[1]) << 8);
  return true;
}

bool readJoystickFrame(uint8_t address, JoystickFrame* frame) {
  if (frame == nullptr) {
    return false;
  }

  // El ejemplo canónico del firmware espera 10 ms después de CMD 0x80.
  if (!requestJoystickFrame(address)) {
    return false;
  }
  sleep_ms(10);
  return receiveJoystickFrame(address, frame);
}

bool requestJoystickFrame(uint8_t address) {
  const uint8_t cmd = CMD_JOYSTICK_FRAME;
  return i2c_write_blocking_until(I2C_PORT, address, &cmd, 1, false,
                                  make_timeout_time_ms(10)) == 1;
}

bool receiveJoystickFrame(uint8_t address, JoystickFrame* frame) {
  if (frame == nullptr) {
    return false;
  }

  uint8_t response[4];
  if (i2c_read_blocking_until(I2C_PORT, address, response, sizeof(response),
                              false, make_timeout_time_ms(10)) !=
      static_cast<int>(sizeof(response))) {
    return false;
  }

  frame->x = (static_cast<uint16_t>(response[0] & 0x0F) << 8) |
             response[1];
  frame->y = (static_cast<uint16_t>(response[2] & 0x0F) << 8) |
             response[3];
  // Compatibilidad del formato: bit7=1 libre. En el hardware GameTiger PA0
  // es el ADC Y, así que el firmware esclavo mantiene este bit siempre en 1.
  frame->switch_pressed = (response[0] & 0x80) == 0;
  return frame->x <= 4095 && frame->y <= 4095;
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

bool readADC_Full(uint8_t address, uint16_t* value) {
  // Lectura usando comandos HSB y LSB separados
  uint8_t hsb, lsb;
  
  // Leer byte alto (HSB)
  if (!readADC_HSB(address, &hsb)) {
    return false;
  }
  
  // Mantener el intervalo usado por la prueba funcional del dispositivo.
  sleep_ms(10);
  
  // Leer byte bajo (LSB)
  if (!readADC_LSB(address, &lsb)) {
    return false;
  }
  
  // Combinar bytes y aplicar máscara de 12 bits (ADC de 12 bits = 0-4095)
  *value = (((uint16_t)hsb << 8) | lsb) & 0x0FFF;
  return true;
}

// ═══════════════════════════════════════════════════════════
//           FUNCIONES DE GESTIÓN DE DIRECCIÓN I2C
// ═══════════════════════════════════════════════════════════

bool setI2CAddress(uint8_t current_addr, uint8_t new_addr) {
  if (new_addr < 0x08 || new_addr > 0x77 || new_addr == current_addr) {
    return false;
  }

  // El joystick usa un setter por etapas: comando, respuesta, dirección en
  // una segunda escritura, respuesta final y reinicio del esclavo.
  uint8_t response = 0;
  if (!sendCommandAndRead(current_addr, CMD_SET_I2C_ADDR, &response) ||
      (response & 0x0F) != RESP_I2C_ADDR_SET) {
    return false;
  }

  int result = i2c_write_blocking_until(I2C_PORT, current_addr, &new_addr, 1,
                                        false, make_timeout_time_ms(50));
  if (result != 1) {
    return false;
  }

  sleep_ms(100);
  result = i2c_read_blocking_until(I2C_PORT, current_addr, &response, 1,
                                   false, make_timeout_time_ms(50));
  if (result != 1 || (response & 0x0F) != RESP_I2C_ADDR_SET) {
    return false;
  }

  sleep_ms(300);
  uint16_t device_id = 0;
  return readJoystickDeviceId(new_addr, &device_id) &&
         device_id == JOYSTICK_DEVICE_ID;
}

bool factoryReset(uint8_t address) {
  return sendCommand(address, CMD_RESET_FACTORY);
}

bool getI2CStatus(uint8_t address, uint8_t* status) {
  return sendCommandAndRead(address, CMD_GET_I2C_STATUS, status);
}
