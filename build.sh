#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-pico2"

export PICO_SDK_PATH="${PICO_SDK_PATH:-/home/mr/rp/pico-sdk}"
export PICO_EXTRAS_PATH="${PICO_EXTRAS_PATH:-/media/mr/firmware/personal/pico/gametiger_console/pico-extras}"

# Mostrar configuración
echo "==========================================="
echo "  Compilación GameTiger Console"
echo "==========================================="
echo "Target: Pico 2 / RP2350 ARM Secure"
echo "PICO_SDK_PATH: $PICO_SDK_PATH"
echo "PICO_EXTRAS_PATH: $PICO_EXTRAS_PATH"
echo "==========================================="

# Limpiar compilación anterior (opcional)
if [ "${1:-}" == "clean" ]; then
    echo "Limpiando compilación anterior..."
    cmake -E remove_directory "$BUILD_DIR"
fi

# Compilar
echo "Iniciando compilación..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DPICO_BOARD=pico2 \
    -DPICO_PLATFORM=rp2350-arm-s \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target GameTiger --parallel "$(nproc)"

# Verificar resultado
FIRMWARE="$BUILD_DIR/GameTiger.uf2"
if [ -f "$FIRMWARE" ]; then
    echo "==========================================="
    echo "✅ Compilación exitosa!"
    echo "Archivo generado: $FIRMWARE ($(stat -c '%s bytes' "$FIRMWARE"))"
    echo "==========================================="
    echo ""
    echo "Para flashear:"
    echo "1. Conecta GameTiger con botón BOOT presionado"
    echo "2. Copia build-pico2/GameTiger.uf2 a la unidad que aparece"
    echo "==========================================="
else
    echo "==========================================="
    echo "❌ Error en la compilación"
    echo "==========================================="
    exit 1
fi
