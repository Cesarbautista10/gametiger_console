#!/bin/bash
# Script para flashear GameTiger al RP2040 usando pyOCD
# Uso: ./flash_pyocd.sh

FIRMWARE_HEX="build/GameTiger.hex"
FIRMWARE_BIN="build/GameTiger.bin"

echo "==========================================="
echo "  GameTiger Flash Tool (pyOCD)"
echo "==========================================="

# Verificar que pyocd está instalado
if ! command -v pyocd &> /dev/null; then
    echo "❌ Error: pyOCD no está instalado"
    echo ""
    echo "Para instalar pyOCD:"
    echo "  pip install pyocd"
    echo "  pip install pyocd-pemicro  # Si usas debug probe PEMicro"
    exit 1
fi

# Verificar que existe el firmware
if [ ! -f "$FIRMWARE_HEX" ]; then
    echo "❌ Error: No se encuentra $FIRMWARE_HEX"
    echo "   Primero ejecuta: ./build.sh"
    exit 1
fi

echo "✅ pyOCD instalado: $(pyocd --version)"
echo "✅ Firmware encontrado: $(ls -lh $FIRMWARE_HEX | awk '{print $5}')"
echo ""

# Listar dispositivos conectados
echo "🔍 Buscando dispositivos conectados..."
pyocd list

echo ""
echo "📋 Nota: Si no aparece ningún dispositivo, verifica:"
echo "   - Que el cable USB esté conectado correctamente"
echo "   - Que tengas permisos para acceder al dispositivo USB"
echo "   - En Linux, puede que necesites ejecutar con sudo"
echo ""
read -p "Presiona ENTER para continuar con el flasheo..."

echo ""
echo "📤 Flasheando firmware al RP2040..."
echo ""

# Flashear usando pyocd
pyocd flash -t rp2040 "$FIRMWARE_HEX"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Firmware flasheado exitosamente"
    echo ""
    echo "🎮 El dispositivo debería reiniciarse automáticamente"
    echo "   y ejecutar el nuevo firmware"
else
    echo ""
    echo "❌ Error al flashear el firmware"
    echo ""
    echo "Troubleshooting:"
    echo "  - Verifica la conexión USB"
    echo "  - Intenta con sudo si es un problema de permisos"
    echo "  - Verifica que el target sea correcto (rp2040)"
    exit 1
fi

echo "==========================================="
