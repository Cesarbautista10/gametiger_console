#pragma once

#include <stdint.h>

namespace HardwareConfig {

// Keep ST7789 commands at the rate already proven on this board. Pixel data
// can use the next safe RP2350 SPI divisor without risking a missed SLPOUT or
// DISPON command and an apparently dead display.
inline constexpr uint32_t DISPLAY_COMMAND_SPI_BAUD_HZ = 20000000;
inline constexpr uint32_t DISPLAY_PIXEL_SPI_BAUD_HZ = 25000000;

}
