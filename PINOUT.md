# 📌 Pinout Completo - GameTiger RP2040

## 🖥️ Display ST7789V2 (Pines del Display)

```
Display ST7789V2          RP2040 GameTiger
┌─────────────┐          ┌──────────────┐
│             │          │              │
│  DIN (MOSI) ├─────────►│  GPIO 3      │  SPI0 TX
│  CK  (SCK)  ├─────────►│  GPIO 2      │  SPI0 SCK
│  CS         ├─────────►│  GPIO 1      │  Chip Select
│  DC         ├─────────►│  GPIO 0      │  Data/Command
│  RST        ├─────────►│  GPIO 4      │  Reset
│  BL         ├─────────►│  GPIO 12*    │  Backlight (opcional)
│             │          │              │
│  VCC        ├─────────►│  3.3V        │
│  GND        ├─────────►│  GND         │
└─────────────┘          └──────────────┘

* BL puede conectarse directo a 3.3V para máximo brillo
```

## 🎮 Botones de Control

```
Botón                     RP2040 GPIO    Pull-up
┌──────────────┐         ┌───────────┐  ┌──────┐
│  D-Pad UP    ├────────►│  GPIO 17  ├──┤ 10kΩ │── VCC
│  D-Pad DOWN  ├────────►│  GPIO 20  ├──┤ 10kΩ │── VCC
│  D-Pad LEFT  ├────────►│  GPIO 19  ├──┤ 10kΩ │── VCC
│  D-Pad RIGHT ├────────►│  GPIO 18  ├──┤ 10kΩ │── VCC
│              │         │           │  └──────┘
│  Botón A     ├────────►│  GPIO 6   ├──┤ 10kΩ │── VCC
│  Botón B     ├────────►│  GPIO 7   ├──┤ 10kΩ │── VCC
│  START       ├────────►│  GPIO 8   ├──┤ 10kΩ │── VCC
│  SELECT      ├────────►│  GPIO 9   ├──┤ 10kΩ │── VCC
└──────────────┘         └───────────┘  
                                        (Pull-up interno activado)
Todos los botones: Activo en BAJO (GND cuando presionado)
```

## 🔊 Audio

```
Amplificador PAM8302A     RP2040
┌──────────────┐         ┌───────────┐
│  Audio IN    ├────────►│  GPIO 5   │  PWM
│  VCC         ├────────►│  3.3V     │
│  GND         ├────────►│  GND      │
│              │         └───────────┘
│  VOUT+       ├────────► Speaker +
│  VOUT-       ├────────► Speaker -
└──────────────┘
```

## 🔋 Batería y Carga

```
Sistema de Batería        RP2040
┌──────────────┐         ┌───────────┐
│  VSYS        ├────────►│  GPIO 26  │  ADC0 (medición voltaje)
│  (Divisor)   │    ┌───►│           │  
│              │    │    └───────────┘
│  USB CHG     ├────┘    
│  Status      ├────────►│  GPIO 16  │  Estado de carga
└──────────────┘         └───────────┘

Divisor de voltaje en VSYS para lectura ADC
```

## 📊 Resumen Completo de Pines

| GPIO | Función | Tipo | Descripción |
|------|---------|------|-------------|
| 0 | DC | OUT | Display Data/Command |
| 1 | CS | OUT | Display Chip Select |
| 2 | SCK | SPI | Display Clock |
| 3 | MOSI | SPI | Display Data |
| 4 | RST | OUT | Display Reset |
| 5 | AUDIO | PWM | Salida de audio |
| 6 | BTN_A | IN | Botón A (pull-up) |
| 7 | BTN_B | IN | Botón B (pull-up) |
| 8 | START | IN | Botón Start (pull-up) |
| 9 | SELECT | IN | Botón Select (pull-up) |
| 10-15 | - | - | **LIBRES** |
| 16 | POWER | IN | Estado de carga |
| 17 | UP | IN | D-Pad Arriba (pull-up) |
| 18 | RIGHT | IN | D-Pad Derecha (pull-up) |
| 19 | LEFT | IN | D-Pad Izquierda (pull-up) |
| 20 | DOWN | IN | D-Pad Abajo (pull-up) |
| 21-25 | - | - | **LIBRES** |
| 26 | VSYS | ADC0 | Nivel de batería |
| 27-28 | - | - | **LIBRES** |

## 🔧 Configuración de Hardware

### SPI0 (Display)
- **Baudrate:** 110 MHz
- **Modo:** CPOL=0, CPHA=0
- **Orden:** MSB First
- **Data Size:** 16-bit para pixel data, 8-bit para comandos

### ADC (Batería)
- **Canal:** ADC0 (GPIO 26)
- **Rango:** 0-3.3V a través de divisor de voltaje
- **Batería Full:** 4.25V
- **Batería Empty:** 2.54V

### PWM (Audio)
- **Pin:** GPIO 5
- **Frecuencia:** Configurable por software

## 💡 Expansiones Posibles

### Pines Disponibles para Expansiones
Si deseas agregar funcionalidades:

- **GPIO 10-15:** I2C, SPI adicional, sensores
- **GPIO 21-25:** LoRa, RF, LEDs adicionales
- **GPIO 27-28:** I2C, entrada adicional
- **GPIO 12:** Backlight display (actualmente no usado)

### Sugerencias de Expansión
1. **Vibración:** GPIO 10
2. **LED RGB:** GPIO 11, 12, 13
3. **Sensor de luz:** GPIO 14 (ADC)
4. **I2C (Giroscopio, Acelerómetro):** GPIO 21 (SDA), GPIO 22 (SCL)
5. **LoRa SX1262:** GPIO 23-25

## ⚠️ Advertencias Importantes

1. **Voltaje:** Todos los pines son de **3.3V** - NO conectar 5V
2. **Corriente máxima por pin:** 12mA
3. **Corriente total GPIO:** 50mA máximo
4. **El display debe ser de 3.3V** o usar level shifter
5. **Batería LiPo:** Requiere circuito de protección y carga (MCP73831)

## 📝 Referencias de Código

Los pines están definidos en:
- Display: `core/display.h` líneas 92-97
- Teclado: `core/keyboard.h` línea 21  
- Batería: `core/battery.h` líneas 9-10
- Audio: `core/common.h` línea 49

---
**Última actualización:** Marzo 2026  
**Hardware:** GameTiger Console v3 con RP2040
