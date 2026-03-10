# Solución para Inversión de Colores en Display ST7789V2

## Problema

Algunos módulos ST7789V2 240×280 requieren inversión de colores (INVON) mientras que otros funcionan correctamente sin ella (INVOFF). Esto causa que los colores se vean invertidos en algunas pantallas.

## Solución

El firmware ahora soporta ambas configuraciones de forma sencilla mediante un `#define` en `core/common.h`.

## Cómo Cambiar la Configuración

### Para la pantalla que muestra colores CORRECTAMENTE:

Edita `core/common.h` y configura:

```c
#define DISPLAY_INVERT_COLORS 1  // INVON activado
```

### Para la pantalla que muestra colores INVERTIDOS:

Edita `core/common.h` y configura:

```c
#define DISPLAY_INVERT_COLORS 0  // INVOFF activado (desactivar inversión)
```

## Pasos Completos

1. **Abre el archivo:**
   ```bash
   nano core/common.h
   ```
   O usa tu editor favorito.

2. **Busca esta línea:**
   ```c
   #define DISPLAY_INVERT_COLORS 1
   ```

3. **Cambia el valor:**
   - `1` = Inversión activada (INVON) - Para pantallas que lo necesitan
   - `0` = Inversión desactivada (INVOFF) - Para pantallas que muestran colores invertidos con INVON

4. **Recompila:**
   ```bash
   make -j$(nproc)
   ```

5. **Flashea el firmware:**
   
   **Opción A - pyOCD (con debug probe):**
   ```bash
   ./flash_pyocd.sh
   ```
   
   **Opción B - UF2 (bootloader):**
   ```bash
   cp GameTiger.uf2 /media/$USER/RPI-RP2/
   ```

## Cómo Identificar Cuál Necesitas

1. **Flashea con el valor por defecto** (`DISPLAY_INVERT_COLORS 1`)
2. **Observa los colores:**
   - ✅ Si se ven correctos → Deja el valor en `1`
   - ❌ Si se ven invertidos (amarillo aparece azul, etc.) → Cambia a `0`

## Ejemplo de Inversión

| Color Real | Con INVON (1) | Con INVOFF (0) |
|------------|---------------|----------------|
| Amarillo   | Amarillo ✓    | Azul ✗         |
| Azul       | Azul ✓        | Amarillo ✗     |
| Rojo       | Rojo ✓        | Cian ✗         |
| Verde      | Verde ✓       | Magenta ✗      |

## Notas Técnicas

- **ST7789_INVON** (0x21): Activa inversión de colores
- **ST7789_INVOFF** (0x20): Desactiva inversión de colores
- Ambos comandos están definidos en `core/display.h`
- La configuración se aplica en `core/display.cpp::initSequence()`

## Variantes de Pantalla Conocidas

Existen al menos dos versiones de módulos ST7789V2 240×280 1.69" en el mercado:
- **Versión A**: Requiere INVON
- **Versión B**: Requiere INVOFF

Ambas son físicamente idénticas pero tienen diferente configuración interna del controlador.
