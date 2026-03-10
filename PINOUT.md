# 📌 Pinout Completo - GameTiger RP2350

## 🖥️ Display ST7789V2 240×320 (Modo Landscape 320×240)

```
Display ST7789V2          RP2350 GameTiger
┌─────────────┐          ┌──────────────┐
│             │          │              │
│  DIN (MOSI) ├─────────►│  GPIO 19     │  SPI0 TX
│  CK  (SCK)  ├─────────►│  GPIO 18     │  SPI0 SCK
│  CS         ├─────────►│  GPIO 15     │  Chip Select
│  DC         ├─────────►│  GPIO 16     │  Data/Command
│  RST        ├─────────►│  GPIO 17     │  Reset
│  BL         ├─────────►│  3.3V        │  Backlight (directo)
│             │          │              │
│  VCC        ├─────────►│  3.3V        │
│  GND        ├─────────►│  GND         │
│  SDA-0      ├─────────►│  GND         │  Forzar modo SPI
└─────────────┘          └──────────────┘

Resolución física: 240×320 pixels
Configuración: Landscape 320×240 (MADCTL=0x68)
```

## 🎮 Botones de Control

```
Botón                     RP2350 GPIO   Pull-up
┌──────────────┐         ┌───────────┐  ┌──────┐
│  D-Pad UP    ├────────►│  GPIO 13  ├──┤ 10kΩ │── VCC
│  D-Pad DOWN  ├────────►│  GPIO 14  ├──┤ 10kΩ │── VCC
│  D-Pad LEFT  ├────────►│  GPIO 20  ├──┤ 10kΩ │── VCC
│  D-Pad RIGHT ├────────►│  GPIO 21  ├──┤ 10kΩ │── VCC
│              │         │           │  └──────┘
│  Botón A     ├────────►│  GPIO 26  ├──┤ 10kΩ │── VCC
│  Botón B     ├────────►│  GPIO 27  ├──┤ 10kΩ │── VCC
│  START       ├────────►│  GPIO 8   ├──┤ 10kΩ │── VCC
│  SELECT      ├────────►│  GPIO 9   ├──┤ 10kΩ │── VCC
└──────────────┘         └───────────┘  
                                        (Pull-up interno activado)
Todos los botones: Activo en BAJO (GND cuando presionado)
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
| 0-4 | - | - | **LIBRES** |
| 5-7 | - | - | **LIBRES** |
| 8 | START | IN | Botón Start (pull-up) |
| 9 | SELECT | IN | Botón Select (pull-up) |
| 10-12 | - | - | **LIBRES** |
| 13 | UP | IN | D-Pad Arriba (pull-up) |
| 14 | DOWN | IN | D-Pad Abajo (pull-up) |
| 15 | CS | OUT | Display Chip Select |
| 16 | DC | OUT | Display Data/Command |
| 17 | RST | OUT | Display Reset |
| 18 | SCK | SPI | Display Clock |
| 19 | MOSI | SPI | Display Data |
| 20 | LEFT | IN | D-Pad Izquierda (pull-up) |
| 21 | RIGHT | IN | D-Pad Derecha (pull-up) |
| 22 | POWER | IN | Estado de carga |
| 23 | AUDIO | PWM | Salida de audio (buzzer/amplificador) |
| 24-25 | - | - | **LIBRES** |
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
