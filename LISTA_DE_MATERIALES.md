# Lista de Materiales (BOM) - GameTiger Console

## 📋 Componentes Principales

### Microcontrolador y Memoria
| Cantidad | Componente | Descripción | Encapsulado |
|----------|-----------|-------------|-------------|
| 1 | **RP2350** | Microcontrolador dual-core ARM Cortex-M33 @ 150MHz | QFN-60 |
| 1 | **W25Q128JVS** (opcional) | Memoria Flash SPI 128Mb (16MB) - RP2350 tiene 4MB interna | SOIC-8 |
| 1 | **Cristal 12MHz** | Cristal para RP2350 con pads GND | SMD |

**Nota**: El RP2350 incluye 520KB SRAM y 4MB de flash QPI interna, por lo que la flash externa es opcional dependiendo de los requisitos del proyecto.

### Reguladores de Voltaje
| Cantidad | Componente | Descripción | Especificaciones |
|----------|-----------|-------------|------------------|
| 1 | **AP2112K-3.3TRG1** | Regulador de voltaje LDO 3.3V | 600mA, SOT-23-3 |
| 1 | **MCP73831-2-OT** | Controlador de carga de batería Li-Po/Li-Ion | 500mA, SOT-23-5 |

### Audio
| Cantidad | Componente | Descripción | Encapsulado |
|----------|-----------|-------------|-------------|
| 1 | **PAM8302A** | Amplificador de audio Clase D 2.5W | MSOP-8 |
| 1 | **Bocina/Speaker** | Altavoz pequeño 8Ω 0.5W-1W | Dependiente del diseño |

### Conectores
| Cantidad | Componente | Descripción | Tipo |
|----------|-----------|-------------|------|
| 1 | **GT-USB-7010ASV** o **503398-1892** | Conector USB Type-C | Receptáculo USB 2.0 |
| 1 | **S2B-PH-SM4-TB** | Conector para batería JST-PH 2 pines | SMD |
| 1 | **503398-1892** | Conector para tarjeta microSD | Molex |
| 1 | **Conector de 15 pines** | Para pantalla LCD | Dependiente del display |

### Pantalla
| Cantidad | Componente | Descripción | Especificaciones |
|----------|-----------|-------------|------------------|
| 1 | **Display LCD ST7789V2** | Pantalla TFT a color | 1.69"-2.0", 320×240, SPI, RGB565 |

### Botones e Interruptores
| Cantidad | Componente | Descripción | Tipo |
|----------|-----------|-------------|------|
| 8-10 | **B3FS-4052P** o similar | Botones táctiles (D-Pad, A, B, Start, Select) | Tact Switch 6x6mm |
| 1 | **MK-12C02-G020** | Interruptor deslizante de encendido/apagado | SPDT, SMD |

### Diodos y Transistores
| Cantidad | Componente | Descripción | Encapsulado |
|----------|-----------|-------------|-------------|
| 1 | **MBR0540** | Diodo Schottky 40V 0.5A | SOD-123 |
| 1 | **MOSFET-P** | MOSFET canal P para protección de batería | SOT-23 |
| 2-3 | **LED SMD** | LEDs para indicadores (carga, encendido, etc.) | 0805 o 1206 |

## 🔌 Componentes Pasivos

### Resistencias SMD (0805 o 0603)
| Cantidad | Valor | Uso |
|----------|-------|-----|
| 2 | 27Ω | USB D+/D- |
| 2 | 1kΩ | Pull-down/up general |
| 5-8 | 10kΩ | Pull-up/pull-down general |
| 2-3 | 100kΩ | Divisor de voltaje, reset |
| 1 | 5.1kΩ (x2) | USB Type-C CC1/CC2 |
| 2-4 | 330Ω - 1kΩ | LEDs |
| 1 | **Potenciómetro trimmer** | Control de brillo o volumen | 10kΩ |

### Capacitores SMD (0805 o 0603)
| Cantidad | Valor | Voltaje | Uso |
|----------|-------|---------|-----|
| 8-12 | 100nF (0.1μF) | 25V | Desacoplamiento |
| 2 | 1μF | 25V | Regulador, filtrado |
| 2 | 10μF | 16V+ | Regulador VCC |
| 2 | 15pF | 50V | Cristal 12MHz |
| 1 | 4.7μF | 16V | MCP73831 |
| 1-2 | 22μF | 16V | Audio PAM8302A |

### Otros Pasivos
| Cantidad | Componente | Descripción |
|----------|-----------|-------------|
| 1 | Ferrite Bead | Filtro EMI para alimentación |

## 🔋 Batería y Alimentación
| Componente | Especificación |
|-----------|----------------|
| **Batería Li-Po** | 3.7V, 500-1200mAh (tamaño según diseño de carcasa) |
| **Cable USB Type-C** | Para programación y carga |

## 🛠️ PCB y Fabricación
| Componente | Especificación |
|-----------|----------------|
| **PCB** | 2 capas (mínimo), acabado HASL o ENIG |
| **Grosor PCB** | 1.6mm estándar |
| **Máscara** | Verde, negra o azul (preferencia) |
| **Serigrafía** | Blanca |

## 🏠 Carcasa (Opcional)
| Componente | Método |
|-----------|--------|
| **Carcasa superior e inferior** | Impresión 3D (archivos .blend disponibles en `/Case`) |
| **Material sugerido** | PLA, PETG o ABS |
| **Tornillos** | M2 o M2.5 x 4-6mm (dependiendo del diseño) |

## 📦 Proveedores Sugeridos

### China/Internacional
- **LCSC/JLCPCB** - Componentes SMD y fabricación PCB
- **Mouser** - Componentes de calidad
- **DigiKey** - Amplio inventario
- **AliExpress** - Conectores, botones, pantallas

### Local (según ubicación)
- Distribuidores locales de componentes electrónicos
- Servicios de impresión 3D locales

## 📝 Notas Importantes

1. **Pantalla LCD**: Panel ST7789V2 de 1.69" (240×280 píxeles) con interfaz SPI - verifica la compatibilidad de los pines con el código antes de comprar
2. **Configuración compilada**: El proyecto ya está configurado para el display ST7789V2 240×280. Usa `./build.sh` para compilar
3. **Batería**: Asegúrate de que el tamaño físico quepa en el diseño de la carcasa
3. **Componentes SMD**: Se recomienda experiencia en soldadura SMD o acceso a servicio de ensamblaje
4. **Programador**: El RP2040 se programa via USB en modo bootloader (no requiere programador externo)
5. **Tarjeta microSD**: Opcional, útil para cargar ROMs de Game Boy

## 🔧 Herramientas Necesarias

- Estación de soldadura con control de temperatura
- Flux para soldadura SMD
- Pinzas de precisión
- Multímetro
- Lupa o microscopio USB (recomendado para SMD)
- Pasta de soldar (para componentes QFN)
- Pistola de aire caliente o plancha de soldadura (para QFN)

## 💰 Costo Aproximado

El costo total estimado de los componentes varía según el proveedor y la cantidad:
- **Un prototipo individual**: $25-40 USD
- **Lote de 10 unidades**: $15-25 USD por unidad
- **PCB fabricado**: $2-10 USD (dependiendo del servicio)

---

**Fecha de actualización**: Marzo 2026  
**Versión del proyecto**: Ver PCB/GameTiger/GameTiger.kicad_sch para detalles exactos

Para más información sobre el ensamblaje y programación, consulta:
- [README.md](README.md)
- [docs/dev_guide.md](docs/dev_guide.md)
