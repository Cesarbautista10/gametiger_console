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

## 🎮 Botones de Control

```
Botones GPIO              RP2350 GPIO   Pull-up
┌──────────────┐         ┌───────────┐  ┌──────┐
│  Botón A     ├────────►│  GPIO 26  ├──┤ 10kΩ │── VCC
│  Botón B     ├────────►│  GPIO 27  ├──┤ 10kΩ │── VCC
│  START       ├────────►│  GPIO 16  ├──┤ 10kΩ │── VCC
│  SELECT      ├────────►│  GPIO 17  ├──┤ 10kΩ │── VCC
└──────────────┘         └───────────┘  └──────┘
                                        (Pull-up interno activado)
Botones GPIO: Activo en BAJO (GND cuando presionado)
```

### D-Pad I2C (Botonera ADC)

```
Botonera I2C              RP2350 I2C0
┌──────────────┐         ┌───────────┐
│  SDA         ├────────►│  GPIO 8   │  I2C0 SDA (pull-up interno)
│  SCL         ├────────►│  GPIO 9   │  I2C0 SCL (pull-up interno)
│  VCC         ├────────►│  3.3V     │
│  GND         ├────────►│  GND      │
└──────────────┘         └───────────┘

Dirección I2C: 0x1A
Frecuencia: 100kHz
Protocolo: CMD (0xD8/0xD9) → STOP → delay 10ms → READ
ADC: 12 bits (0-4095)

Rangos de voltaje (3.3V referencia):
- UP:    2150-2550  (~2350, 1.9V)
- DOWN:     0-200   (~4,    0.0V)
- LEFT:   600-1000  (~792,  0.6V)
- RIGHT: 1450-1850  (~1649, 1.3V)
- NONE:  3900-4095  (~4088, 3.3V)
```

## 🔊 Audio

```
Amplificador PAM8302A     RP2350
┌──────────────┐         ┌───────────┐
│  Audio IN    ├────────►│  GPIO 23  │  PWM
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
| 16 | START | IN | Botón Start (pull-up interno, activo LOW) |
| 17 | SELECT | IN | Botón Select (pull-up interno, activo LOW) |
| 18 | RST | OUT | Display Reset (D1) |
| 19-21 | - | - | **LIBRES** |
| 22 | CHG_STAT | IN | Estado de carga batería |
| 23 | AUDIO_PWM | OUT | PWM para amplificador PAM8302A |
| 24-25 | - | - | **LIBRES** |
| 26 | BTN_A | IN | Botón A (pull-up interno, activo LOW) |
| 27 | BTN_B | IN | Botón B (pull-up interno, activo LOW) |
| 28 | ADC_VSYS | ADC | Medición voltaje batería (ADC2) |
| 29 | - | - | **LIBRE** |

### Notas:
- **D-Pad (UP/DOWN/LEFT/RIGHT)**: Controlado via I2C (GPIO 8/9) en dirección 0x1A
- **Botones activo LOW**: Conectar botón entre GPIO y GND (presionado = LOW)
- **Pull-ups**: Todos los botones GPIO usan resistencias pull-up internas activadas
- **SPI Display**: SPI1 @ 110 MHz para pantalla ST7789V2
| 21 | - | - | **LIBRE** (antes D-Pad RIGHT) |
| 22 | POWER | IN | Estado de carga |
| 23 | AUDIO | PWM | Salida de audio (buzzer/amplificador) |
| 24 | SDA | I2C0 | D-Pad I2C Data (botonera @ 0x56) |
| 25 | SCL | I2C0 | D-Pad I2C Clock (botonera @ 0x56) |
| 26 | BTN_A | IN | Botón A (pull-up) |
| 27 | BTN_B | IN | Botón B (pull-up) |
| 28 | VSYS | ADC | Nivel de batería |
| 29-47 | - | - | **LIBRES** (RP2350 extendido) |

## 🔧 Configuración de Hardware

### SPI0 (Display)
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
- **Pin:** GPIO 23
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
- Audio: `core/common.h` línea 54 (GPIO 23)

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
