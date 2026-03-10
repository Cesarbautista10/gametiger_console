#!/usr/bin/env python3
from PIL import Image

# Cargar la imagen original
img = Image.open('logo.png')
print(f'Imagen original: {img.size}, modo: {img.mode}')

# Redimensionar a 128x128 con alta calidad
img_resized = img.resize((128, 128), Image.LANCZOS)
print(f'Imagen redimensionada: {img_resized.size}')

# Convertir a RGBA si no lo está
if img_resized.mode != 'RGBA':
    img_resized = img_resized.convert('RGBA')

print('Imagen mantenida en blanco (sin invertir colores)')

# Guardar la imagen procesada
output_path = 'content/image_util/menu/tiger.png'
img_resized.save(output_path)
print(f'Imagen guardada en: {output_path}')

# Verificar resultado
img_final = Image.open(output_path)
print(f'Verificación: {img_final.size}, modo: {img_final.mode}')
