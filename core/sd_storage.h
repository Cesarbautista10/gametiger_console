#pragma once

#include <stddef.h>
#include <stdint.h>

enum class SDLoadResult : uint8_t {
    OK,
    CARD_NOT_READY,
    FILESYSTEM_UNSUPPORTED,
    MOUNT_FAILED,
    OPEN_FAILED,
    INVALID_SIZE,
    OUT_OF_MEMORY,
    READ_FAILED,
};

constexpr size_t SD_MAX_GAME_BOY_ROMS = 32;
constexpr size_t SD_GAME_BOY_FILENAME_SIZE = 13;
constexpr size_t SD_GAME_BOY_PATH_SIZE = 20;

struct SDGameBoyROM {
    char filename[SD_GAME_BOY_FILENAME_SIZE];
    char path[SD_GAME_BOY_PATH_SIZE];
    uint32_t size;
};

// Refreshes the fixed-size catalog from /roms. Names use FAT short-name
// format because this build intentionally disables long-file-name support.
size_t sdScanGameBoyROMs(SDLoadResult *result);
size_t sdGameBoyROMCount();
const SDGameBoyROM *sdGameBoyROMAt(size_t index);

// Loads a complete file into heap memory. The caller owns the returned buffer
// and must release it with free(). Returns nullptr on failure.
uint8_t *sdLoadFile(const char *path, size_t *size, SDLoadResult *result);

const char *sdLoadResultText(SDLoadResult result);
