# Cambios para Display ST7789V2 240×280

Este documento describe los cambios realizados para adaptar GameTiger al nuevo display ST7789V2 de 1.69" con resolución 240×280 píxeles.

## 🔧 Cambios Realizados

### 1. Configuración de Resolución
**Archivo:** `core/common.h`

```cpp
// Antes (Display original 320×240)
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Ahora (Display ST7789V2 240×280)
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280
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

El display ST7789V2 tiene los siguientes pines (DIN, CK, CS, DC, RST, BL):

| Pin Display | Pin RP2040 | Función | Definido en código |
|------------|-----------|---------|-------------------|
| **DIN** (MOSI) | **GPIO 3** | SPI Data In | `MOSI_PIN = 3` |
| **CK** (SCK) | **GPIO 2** | SPI Clock | `SCK_PIN = 2` |
| **CS** | **GPIO 1** | Chip Select | `CS_PIN = 1` |
| **DC** | **GPIO 0** | Data/Command | `DC_PIN = 0` |
| **RST** | **GPIO 4** | Reset | `RST_PIN = 4` |
| **BL** | GPIO 12 (opcional) | Backlight | `BL_PIN = 12` (comentado) |
| **VCC** | **3.3V** | Alimentación | - |
| **GND** | **GND** | Tierra | - |

### Notas sobre Backlight (BL)
El pin de backlight está actualmente **deshabilitado** en el código. Si tu display requiere control de brillo:

1. Conecta BL a **GPIO 12**
2. Descomenta las líneas en `core/display.cpp`:
   ```cpp
   // Líneas 23-25 y 138
   gpio_init(BL_PIN);
   gpio_set_dir(BL_PIN, GPIO_OUT);
   gpio_put(BL_PIN, 1);
   ```

O simplemente conecta **BL directamente a 3.3V** para máximo brillo constante.

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

### Colores incorrectos
- El formato de color es RGB565
- Verifica el valor de MADCTL para la rotación correcta

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
