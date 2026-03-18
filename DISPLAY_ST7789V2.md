# Display ST7789V2 - GameTiger Console

Este documento describe la configuración del display ST7789V2 usado en GameTiger Console con RP2350.

## 📺 Especificaciones del Display

| Característica | Valor |
|---------------|-------|
| **Controlador** | ST7789V2 |
| **Resolución** | 320×240 píxeles (landscape) |
| **Tamaño** | 1.69" - 2.0" diagonal |
| **Interfaz** | SPI1 @ 110 MHz |
| **Voltaje** | 3.3V |
| **Color** | RGB565 (16-bit, 65K colores) |

## 🔧 Configuración Actual

### Resolución
**Archivo:** `core/common.h`

```cpp
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
```

### 2. Driver Compatible
El driver ST7789 ya implementado en `core/display.cpp` es **100% compatible** con el ST7789V2. No se requieren cambios adicionales en el código del driver.

### 3. Compilación

#### Compilación Rápida
```bash
./build.sh
```

#### Compilación Manual
```bash
export PICO_SDK_PATH=/home/mr/pico/pico-sdk
export PICO_EXTRAS_PATH=/home/mr/pico/pico-extras
export PICO_TOOLCHAIN_PATH=/usr

cmake .
make -j$(nproc)
```

El firmware compilado se generará como `GameTiger.uf2` (aprox. 1.2MB).

## 📱 Especificaciones del Display

| Característica | Valor |
|---------------|-------|
| **Controlador** | ST7789V2 |
| **Tamaño** | 1.69 pulgadas |
| **Resolución** | 240×280 píxeles |
| **Interfaz** | SPI |
| **Voltaje** | 3.3V |
| **Colores** | RGB565 (65K colores) |

## 🔌 Conexiones del Display

El display ST7789V2 se conecta al RP2350 mediante SPI1:

| Pin Display | Pin RP2350 | Función | Definido en código |
|------------|-----------|---------|-------------------|
| **DIN** (MOSI) | **GPIO 15** | SPI1 TX (D4) | `MOSI_PIN = 15` |
| **CK** (SCK) | **GPIO 14** | SPI1 Clock (D5) | `SCK_PIN = 14` |
| **CS** | **GPIO 13** | Chip Select (D6) | `CS_PIN = 13` |
| **DC** | **GPIO 12** | Data/Command (D7) | `DC_PIN = 12` |
| **RST** | **GPIO 18** | Reset (D1) | `RST_PIN = 18` |
| **BL** | 3.3V o GPIO 12 | Backlight | Opcional, comentado en código |
| **VCC** | **3.3V** | Alimentación | - |
| **GND** | **GND** | Tierra | - |

**Archivo de configuración**: `core/display.h` (líneas 108-112)

### Notas sobre Backlight (BL)
El pin de backlight está actualmente **deshabilitado** en el código para ahorrar un GPIO. Opciones:

**Opción 1**: Conecta **BL directamente a 3.3V** para máximo brillo constante (recomendado).

**Opción 2**: Si necesitas control PWM de brillo:
1. Define un GPIO disponible (ej: GPIO 11)
2. Descomenta las líneas en `core/display.cpp`:
   ```cpp
   // Líneas 23-25
   const uint8_t BL_PIN = 11;
   gpio_init(BL_PIN);
   gpio_set_dir(BL_PIN, GPIO_OUT);
   gpio_put(BL_PIN, 1);
   ```

## 🔨 Compilación

### Usando build script
```bash
./build.sh
```

### Compilación manual
```bash
export PICO_SDK_PATH=$(pwd)/pico-sdk
export PICO_EXTRAS_PATH=$(pwd)/pico-extras
export PICO_TOOLCHAIN_PATH=/usr

mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

El firmware se genera como `build/GameTiger.uf2` (~1.2MB).

## 📦 Flashear el Firmware

1. **Conectar en modo bootloader:**
   - Mantén presionado el botón **BOOT** del GameTiger
   - Conecta el cable USB
   - Suelta el botón BOOT
   - Debería aparecer como unidad de almacenamiento (RPI-RP2)

2. **Copiar firmware:**
   - Arrastra `GameTiger.uf2` a la unidad RPI-RP2
   - El dispositivo se desconectará automáticamente al completar
   - Se reiniciará con el nuevo firmware

3. **Verificar:**
   - El display debería mostrar la pantalla de inicio
   - Resolución: 240×280 píxeles

## ⚠️ Consideraciones

### Rotación del Display
El display está configurado con rotación mediante el registro MADCTL:
```cpp
this->sendData(ST7789_MADCTL, (uint8_t)0x78);  // Rotación actual
```

Si necesitas cambiar la orientación, modifica este valor en `core/display.cpp` línea 53:
- `0x00` - Normal
- `0x60` - 90° rotación
- `0xC0` - 180° rotación
- `0xA0` - 270° rotación
- `0x78` - Actual (orientación vertical)

### Rendimiento
- La resolución 240×280 = 67,200 píxeles (vs 76,800 del original)
- **Mejor rendimiento**: ~5% menos píxeles para renderizar
- **FPS esperado**: 35-50 FPS (dependiendo del juego)
- **Baudrate SPI**: 110 MHz configurado en `display.cpp`

### Ajustes de Juegos
Algunos juegos pueden necesitar ajustes menores en las posiciones de elementos de UI debido al cambio de resolución. Revisa especialmente:
- Menús centrados
- Posición de sprites de UI
- Áreas de juego

## 🐛 Solución de Problemas

### Pantalla en blanco
- Verifica las conexiones SPI
- Revisa que el voltaje sea 3.3V
- Confirma que el pin RST esté conectado

### Colores incorrectos o invertidos
El ST7789V2 viene en diferentes variantes de fabricación. Algunos módulos requieren inversión de colores y otros no.

**Solución rápida:**
1. Edita `core/common.h`
2. Busca la línea:
   ```c
   #define DISPLAY_INVERT_COLORS 1
   ```
3. Cambia el valor:
   - `1` = Inversión activada (ST7789_INVON) - Pantalla que funciona correctamente
   - `0` = Inversión desactivada (ST7789_INVOFF) - Pantalla que muestra colores invertidos

4. Recompila y flashea:
   ```bash
   make -j$(nproc)
   ./flash_pyocd.sh
   # o
   cp GameTiger.uf2 /media/$USER/RPI-RP2/
   ```

Ver [DISPLAY_COLOR_FIX.md](DISPLAY_COLOR_FIX.md) para más detalles sobre este problema.

### Pantalla invertida/rotada
- Ajusta el valor MADCTL en `display.cpp` línea 53

### Compilación falla
- Verifica que estén instalados: `cmake`, `build-essential`, `gcc-arm-none-eabi`
- Confirma que `PICO_SDK_PATH` y `PICO_EXTRAS_PATH` estén configurados
- Ejecuta `./build.sh clean` para limpiar y recompilar

## 📚 Referencias

- [Datasheet ST7789V2](https://www.rhydolabz.com/documents/33/ST7789.pdf)
- [RP2040 Hardware Design Guide](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)

## 📅 Historial de Cambios

- **2026-03-06**: Adaptación inicial para ST7789V2 240×280
  - Actualizada resolución en `core/common.h`
  - Creado script de compilación `build.sh`
  - Actualizada lista de materiales

---

**Nota**: El código del driver ST7789 es totalmente compatible con el ST7789V2. Solo fue necesario cambiar las constantes de resolución.
