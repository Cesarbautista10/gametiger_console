#pragma once

#include <stddef.h>
#include <stdint.h>

// Small wrapper around pico_flash that guarantees interrupts are disabled and
// the unused second core is treated as safe while XIP is temporarily stopped.
bool flashStorageErase(uint32_t flashOffset, size_t size);
bool flashStorageProgram(uint32_t flashOffset, const uint8_t *data, size_t size);

