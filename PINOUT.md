# 📌 Pinout Completo - GameTiger RP2350

## 🖥️ Display ST7789V2 320×240

```
Display ST7789V2          RP2350 GameTiger
┌─────────────┐          ┌──────────────┐
│             │          │              │
│  DIN (MOSI) ├─────────►│  GPIO 15     │  SPI1 TX (D4)
│  CK  (SCK)  ├─────────►│  GPIO 14     │  SPI1 SCK (D5)
│  CS         ├─────────►│  GPIO 13     │  Chip Select (D6)
│  DC         ├─────────►│  GPIO 12     │  Data/Command (D7)
│  RST        ├─────────►│  GPIO 18     │  Reset (D1)
│  BL         ├─────────►│  3.3V        │  Backlight (directo o GPIO 12)
│             │          │              │
│  VCC        ├─────────►│  3.3V        │
│  GND        ├─────────►│  GND         │
└─────────────┘          └──────────────┘

Resolución: 320×240 pixels (landscape)
Interfaz: SPI1 @ 110 MHz
Color: RGB565 (16-bit, 65K colores)
Driver: ST7789V2 compatible
```

## 🎮 Joystick I2C (dos escaleras ADC)

```
Botonera I2C              RP2350 I2C0
┌──────────────┐         ┌───────────┐
│  SDA         ├────────►│  GPIO 8   │  I2C0 SDA (pull-up interno)
│  SCL         ├────────►│  GPIO 9   │  I2C0 SCL (pull-up interno)
│  VCC         ├────────►│  3.3V     │
│  GND         ├────────►│  GND      │
└──────────────┘         └───────────┘
```

Dirección I2C configurada en GameTiger: 0x53 (7 bits)
Dirección I2C de fábrica: 0x20 (7 bits)
Dirección persistida: cualquiera entre 0x08 y 0x77; GameTiger la detecta
Device ID DDP: 0x0101
Frecuencia: 100kHz
Protocolo: WRITE 0x80 → STOP → delay 10ms → READ 4 bytes
Trama: [XH+R, XL, YH, YL], con bit 7 reservado siempre en 1

Entradas físicas en el microcontrolador de la botonera:

- X = PA1/ADC_IN1: B, START, A, SELECT
- Y = PA0/ADC_IN0: UP, RIGHT, LEFT, DOWN
- PA2 no se usa para la botonera en esta revisión de hardware

Cada ADC es de 12 bits (0-4095). Una escalera permite una tecla de su grupo a
la vez; como X e Y son canales separados, sí se puede pulsar una dirección y
un botón de acción simultáneamente.

Ventanas iniciales usadas por ambos canales (se configuran por separado en
`core/keyboard.h`):

- Slot 1: 0-500
- Slot 2: 600-1500
- Slot 3: 1550-2350
- Slot 4: 2400-3300
- Ninguna tecla: fuera de esos rangos (normalmente cerca de 4095)

Mapeo lógico de la consola:

- Y 0-500: UP
- Y 600-1500: RIGHT
- Y 1550-2350: LEFT
- Y 2400-3300: DOWN
- X 0-500: B
- X 600-1500: START
- X 1550-2350: A
- X 2400-3300: SELECT

## 🔊 Audio

```
Amplificador PAM8302A     RP2350
┌──────────────┐         ┌───────────┐
│  Audio IN    ├────────►│  GPIO 16  │  PWM
│  VCC         ├────────►│  3.3V     │
│  GND         ├────────►│  GND      │
│              │         └───────────┘
│  VOUT+       ├────────► Speaker +
│  VOUT-       ├────────► Speaker -
└──────────────┘
```

## 🔋 Batería y Carga

```
Sistema de Batería        RP2350
┌──────────────┐         ┌───────────┐
│  VSYS        ├────────►│  GPIO 28  │  ADC (medición voltaje)
│  (Divisor)   │    ┌───►│           │  
│              │    │    └───────────┘
│  USB CHG     ├────┘    
│  Status      ├────────►│  GPIO 22  │  Estado de carga
└──────────────┘         └───────────┘

Divisor de voltaje en VSYS para lectura ADC
```

## 📊 Resumen Completo de Pines

| GPIO | Función | Tipo | Descripción |
|------|---------|------|-------------|
| 0-7 | - | - | **LIBRES** |
| 8 | I2C0 SDA | I2C | D-Pad I2C Data (pull-up interno) |
| 9 | I2C0 SCL | I2C | D-Pad I2C Clock (pull-up interno) |
| 10-11 | - | - | **LIBRES** |
| 12 | DC | OUT | Display Data/Command (D7) |
| 13 | CS | OUT | Display Chip Select (D6) |
| 14 | SCK (SPI1) | OUT | Display SPI Clock (D5) |
| 15 | MOSI (SPI1) | OUT | Display SPI TX (D4) |
| 16 | AUDIO_PWM | OUT | PWM para amplificador PAM8302A |
| 17 | - | - | No usado por la botonera I2C |
| 18 | RST | OUT | Display Reset (D1) |
| 19-21 | - | - | **LIBRES** |
| 22 | CHG_STAT | IN | Estado de carga batería |
| 23 | - | - | No usado por el audio en esta rama |
| 24-25 | - | - | **LIBRES** |
| 26-27 | - | - | No usados por la botonera I2C |
| 28 | ADC_VSYS | ADC | Medición voltaje batería (ADC2) |
| 29 | - | - | Libre tras mover el audio a GPIO16 |

### Notas:

- **Los ocho controles** llegan por I2C (GPIO 8/9) desde dos escaleras ADC
- **Dirección**: 0x53 preferida, 0x20 de fábrica o cualquier dirección persistida detectada por Device ID
- **SPI Display**: SPI1 @ 110 MHz para pantalla ST7789V2

## 🔧 Configuración de Hardware

### SPI1 (Display)
- **Baudrate:** 110 MHz
- **Modo:** CPOL=0, CPHA=0
- **Orden:** MSB First
- **Data Size:** 16-bit para pixel data, 8-bit para comandos

### ADC (Batería)
- **Canal:** ADC (GPIO 28)
- **Rango:** 0-3.3V a través de divisor de voltaje
- **Batería Full:** 4.25V
- **Batería Empty:** 2.54V

### PWM (Audio)
- **Pin:** GPIO 16
- **Frecuencia:** Configurable por software

## 💡 Expansiones Posibles

### Pines Disponibles para Expansiones
Si deseas agregar funcionalidades:

- **GPIO 0-4:** I2C, SPI adicional, sensores
- **GPIO 6-7:** Entrada adicional, LEDs
- **GPIO 10-12:** LoRa, RF, sensores
- **GPIO 23-25:** I2C, SPI, expansiones
- **GPIO 29-47:** Expansiones adicionales RP2350

### Sugerencias de Expansión
1. **Vibración:** GPIO 10
2. **LED RGB:** GPIO 11, 12, 0
3. **Sensor de luz:** GPIO 29 (ADC)
4. **I2C (Giroscopio, Acelerómetro):** GPIO 6 (SDA), GPIO 7 (SCL)
5. **LoRa SX1262:** GPIO 23-25
6. **Backlight controlado:** GPIO 12 (PWM)

## ⚠️ Advertencias Importantes

1. **Voltaje:** Todos los pines son de **3.3V** - NO conectar 5V
2. **Corriente máxima por pin:** 12mA
3. **Corriente total GPIO:** 50mA máximo
4. **El display debe ser de 3.3V** o usar level shifter
5. **Batería LiPo:** Requiere circuito de protección y carga (MCP73831)
6. **RP2350:** Asegúrate de usar la versión correcta del SDK (Pico SDK 2.0+)

## 📝 Referencias de Código

Los pines están definidos en:
- Display: `core/display.h` líneas 92-97
- Teclado: `core/keyboard.h` línea 21  
- Batería: `core/battery.h` líneas 9-10
- Audio: `core/common.h` línea 54 (GPIO 16)

## 🔄 Configuración de Compilación

Para compilar para RP2350:
```bash
cd build
cmake .. -DRPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2
make -j$(nproc)
```

Para flashear con pyOCD:
```bash
./flash_pyocd.sh
```

---
**Última actualización:** Marzo 2025 - RP2350 Branch  
**Hardware:** GameTiger Console v3 con RP2040
