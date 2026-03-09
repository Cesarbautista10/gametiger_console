#!/bin/bash
# Script de compilación para GameTiger Console
# Configurado para Display ST7789V2 240×280

# Configurar variables de entorno
export PICO_SDK_PATH=/home/mr/pico/pico-sdk
export PICO_EXTRAS_PATH=/home/mr/pico/pico-extras
export PICO_TOOLCHAIN_PATH=/usr

# Mostrar configuración
echo "==========================================="
echo "  Compilación GameTiger Console"
echo "==========================================="
echo "Display: ST7789V2 240×280 píxeles"
echo "PICO_SDK_PATH: $PICO_SDK_PATH"
echo "PICO_EXTRAS_PATH: $PICO_EXTRAS_PATH"
echo "==========================================="

# Limpiar compilación anterior (opcional)
if [ "$1" == "clean" ]; then
    echo "Limpiando compilación anterior..."
    rm -rf CMakeFiles CMakeCache.txt *.a *.elf *.bin *.uf2 *.hex *.map
fi

# Compilar
echo "Iniciando compilación..."
cmake . && make -j$(nproc)

# Verificar resultado
if [ -f "GameTiger.uf2" ]; then
    echo "==========================================="
    echo "✅ Compilación exitosa!"
    echo "Archivo generado: $(ls -lh GameTiger.uf2 | awk '{print $9, $5}')"
    echo "==========================================="
    echo ""
    echo "Para flashear:"
    echo "1. Conecta GameTiger con botón BOOT presionado"
    echo "2. Copia GameTiger.uf2 a la unidad que aparece"
    echo "==========================================="
else
    echo "==========================================="
    echo "❌ Error en la compilación"
    echo "==========================================="
    exit 1
fi
