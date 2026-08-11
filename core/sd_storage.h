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
    WRITE_FAILED,
    SAVE_DAMAGED,
    SAVE_RECOVERED,
    FLASH_CACHE_FAILED,
};

constexpr size_t SD_MAX_GAME_BOY_ROMS = 32;
constexpr size_t SD_GAME_BOY_FILENAME_SIZE = 13;
constexpr size_t SD_GAME_BOY_PATH_SIZE = 20;
constexpr size_t SD_MIN_GAME_BOY_ROM_SIZE = 32 * 1024;
constexpr size_t SD_MAX_GAME_BOY_ROM_SIZE = 1024 * 1024;

struct SDGameBoyROM {
    char filename[SD_GAME_BOY_FILENAME_SIZE];
    char path[SD_GAME_BOY_PATH_SIZE];
    uint32_t size;
    uint16_t modifiedDate;
    uint16_t modifiedTime;
};

// Refreshes the fixed-size catalog from /roms. Names use FAT short-name
// format because this build intentionally disables long-file-name support.
size_t sdScanGameBoyROMs(SDLoadResult *result);
size_t sdGameBoyROMCount();
const SDGameBoyROM *sdGameBoyROMAt(size_t index);

// Loads a complete file into heap memory. The caller owns the returned buffer
// and must release it with free(). Returns nullptr on failure.
uint8_t *sdLoadFile(const char *path, size_t *size, SDLoadResult *result);

// Reads an exact range without retaining an open file. This is used to inspect
// a ROM header before deciding whether it can live in SRAM or needs XIP flash.
bool sdReadFileRange(const char *path, size_t offset, uint8_t *buffer,
                     size_t length, size_t *fileSize, SDLoadResult *result);

// Calculates an exact whole-file CRC32 using a small SRAM buffer. This lets
// the XIP cache prove that its contents still match the current SD file.
bool sdCalculateFileCRC32(const char *path, size_t expectedSize,
                          uint32_t *crc32, SDLoadResult *result);

// Copies a ROM sequentially from microSD into an erased, page-aligned flash
// region and verifies every programmed chunk. The destination is an offset
// from XIP_BASE, not an absolute address.
bool sdStageFileInFlash(const char *path, size_t expectedSize,
                        uint32_t flashOffset, uint32_t *crc32,
                        SDLoadResult *result);

// Cartridge save helpers. A missing save is reported as OK with found=false.
// A malformed save starts clean only after both CRC-checked generations are
// exhausted. SAVE_DAMAGED/SAVE_RECOVERED let the UI warn the player; I/O
// errors fail rather than silently rolling back to an older generation.
bool sdLoadSaveFile(const char *path, uint8_t *buffer, size_t size,
                    bool *found, SDLoadResult *result);
bool sdWriteSaveFile(const char *path, const uint8_t *buffer, size_t size,
                     SDLoadResult *result);

const char *sdLoadResultText(SDLoadResult result);
