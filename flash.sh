#!/bin/bash
# Script para flashear GameTiger.uf2 al RP2040
# Uso: ./flash.sh

FIRMWARE="GameTiger.uf2"
MOUNT_POINT="/media/$USER/RPI-RP2"

echo "==========================================="
echo "  GameTiger Flash Tool"
echo "==========================================="

# Verificar que existe el firmware
if [ ! -f "$FIRMWARE" ]; then
    echo "❌ Error: No se encuentra $FIRMWARE"
    echo "   Primero ejecuta: ./build.sh"
    exit 1
fi

echo "✅ Firmware encontrado: $(ls -lh $FIRMWARE | awk '{print $5}')"
echo ""
echo "📋 Instrucciones:"
echo "   1. Desconecta el GameTiger (si está conectado)"
echo "   2. Mantén presionado el botón BOOT"
echo "   3. Conecta el USB mientras mantienes BOOT"
echo "   4. Suelta el botón BOOT"
echo ""
read -p "Presiona ENTER cuando aparezca la unidad RPI-RP2..."

# Esperar a que aparezca el dispositivo
echo ""
echo "🔍 Esperando dispositivo RPI-RP2..."
TIMEOUT=30
COUNT=0

while [ ! -d "$MOUNT_POINT" ] && [ $COUNT -lt $TIMEOUT ]; do
    sleep 1
    COUNT=$((COUNT + 1))
    echo -n "."
done

echo ""

if [ ! -d "$MOUNT_POINT" ]; then
    echo "❌ Error: No se detectó RPI-RP2 en $TIMEOUT segundos"
    echo ""
    echo "Verifica que:"
    echo "  - El GameTiger esté en modo bootloader"
    echo "  - El cable USB funcione correctamente"
    echo "  - El puerto USB esté activo"
    echo ""
    echo "Intenta manualmente:"
    echo "  1. ls /media/$USER/"
    echo "  2. cp $FIRMWARE /media/\$USER/RPI-RP2/"
    exit 1
fi

echo "✅ Dispositivo detectado en: $MOUNT_POINT"
echo ""
echo "📤 Copiando firmware..."

cp "$FIRMWARE" "$MOUNT_POINT/"

if [ $? -eq 0 ]; then
    echo "✅ Firmware copiado exitosamente"
    echo ""
    echo "⏳ El dispositivo se reiniciará automáticamente..."
    echo "   Espera unos segundos..."
    sleep 3
    
    if [ ! -d "$MOUNT_POINT" ]; then
        echo "✅ GameTiger reiniciado correctamente"
        echo ""
        echo "🎮 El dispositivo debería estar ejecutando el nuevo firmware"
    else
        echo "⚠️  El dispositivo aún está montado"
        echo "   Desconecta y reconecta el USB"
    fi
else
    echo "❌ Error al copiar el firmware"
    exit 1
fi

echo "==========================================="
