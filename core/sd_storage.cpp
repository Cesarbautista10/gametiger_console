#include "sd_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "spi_bus_config.h"

extern "C" {
#include "ff.h"
#include "f_util.h"
}

namespace {
constexpr uint LCD_CS_PIN = 21;
constexpr uint SD_CS_PIN = 7;
// A conservative rate makes the first hardware bring-up tolerant of longer
// PCB traces and simple microSD breakouts. Loading a 32 KiB ROM is still fast.
constexpr uint32_t SD_SPI_BAUD = 4000000;
constexpr size_t MIN_GB_ROM_SIZE = 32 * 1024;
// ROMs are loaded completely into the RP2350's 512 KiB SRAM. Keep enough
// memory for the framebuffer, emulator state and optional cartridge RAM.
constexpr size_t MAX_GB_ROM_SIZE = 256 * 1024;

// Keep FatFs work objects out of the Pico SDK's small default stack.
FATFS sdFilesystem;
FIL sdFile;
DIR sdDirectory;
FILINFO sdFileInfo;
SDGameBoyROM gameBoyROMs[SD_MAX_GAME_BOY_ROMS];
size_t gameBoyROMCount = 0;

void setMountFailure(FRESULT fr, SDLoadResult *result) {
    if (!result)
        return;
    if (fr == FR_NOT_READY)
        *result = SDLoadResult::CARD_NOT_READY;
    else if (fr == FR_NO_FILESYSTEM)
        *result = SDLoadResult::FILESYSTEM_UNSUPPORTED;
    else
        *result = SDLoadResult::MOUNT_FAILED;
}

bool isGameBoyROMName(const char *name) {
    const size_t length = strlen(name);
    return length > 3 && name[length - 3] == '.' &&
        tolower(static_cast<unsigned char>(name[length - 2])) == 'g' &&
        tolower(static_cast<unsigned char>(name[length - 1])) == 'b';
}

int compareNamesIgnoreCase(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        const int leftChar = tolower(static_cast<unsigned char>(*left));
        const int rightChar = tolower(static_cast<unsigned char>(*right));
        if (leftChar != rightChar)
            return leftChar - rightChar;
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) -
           static_cast<unsigned char>(*right);
}

void insertGameBoyROM(const SDGameBoyROM &entry) {
    size_t position = gameBoyROMCount;
    while (position > 0 &&
           compareNamesIgnoreCase(
               entry.filename, gameBoyROMs[position - 1].filename) < 0) {
        gameBoyROMs[position] = gameBoyROMs[position - 1];
        --position;
    }
    gameBoyROMs[position] = entry;
    ++gameBoyROMCount;
}

void waitForSPI() {
    while (spi_is_busy(spi0))
        tight_loop_contents();
    while (spi_is_readable(spi0))
        (void)spi_get_hw(spi0)->dr;
}

class DisplaySPIStateGuard {
public:
    DisplaySPIStateGuard() {
        waitForSPI();
        // The LCD and SD card use the same SPI0 controller on separate pin
        // groups. Never address the SD card while the LCD chip select is low.
        gpio_put(LCD_CS_PIN, 1);
        gpio_put(SD_CS_PIN, 1);
        spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        spi_set_baudrate(spi0, SD_SPI_BAUD);
    }

    ~DisplaySPIStateGuard() {
        waitForSPI();
        gpio_put(SD_CS_PIN, 1);
        spi_set_baudrate(spi0, HardwareConfig::DISPLAY_PIXEL_SPI_BAUD_HZ);
        spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }
};
}

size_t sdScanGameBoyROMs(SDLoadResult *result) {
    if (result)
        *result = SDLoadResult::READ_FAILED;
    gameBoyROMCount = 0;
    memset(gameBoyROMs, 0, sizeof(gameBoyROMs));

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdDirectory, 0, sizeof(sdDirectory));
    memset(&sdFileInfo, 0, sizeof(sdFileInfo));

    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        printf("[SD] Catalog mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        setMountFailure(fr, result);
        return 0;
    }

    fr = f_opendir(&sdDirectory, "/roms");
    if (fr != FR_OK) {
        printf("[SD] f_opendir(/roms) failed: %s (%d)\n",
               FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::OPEN_FAILED;
        f_unmount("");
        return 0;
    }

    while (gameBoyROMCount < SD_MAX_GAME_BOY_ROMS) {
        fr = f_readdir(&sdDirectory, &sdFileInfo);
        if (fr != FR_OK || sdFileInfo.fname[0] == '\0')
            break;
        if ((sdFileInfo.fattrib & AM_DIR) != 0 ||
            !isGameBoyROMName(sdFileInfo.fname) ||
            sdFileInfo.fsize < MIN_GB_ROM_SIZE ||
            sdFileInfo.fsize > MAX_GB_ROM_SIZE ||
            (sdFileInfo.fsize % (16 * 1024)) != 0) {
            continue;
        }

        SDGameBoyROM entry = {};
        snprintf(entry.filename, sizeof(entry.filename), "%s",
                 sdFileInfo.fname);
        snprintf(entry.path, sizeof(entry.path), "/roms/%s",
                 sdFileInfo.fname);
        entry.size = static_cast<uint32_t>(sdFileInfo.fsize);
        insertGameBoyROM(entry);
    }

    f_closedir(&sdDirectory);
    f_unmount("");

    if (fr != FR_OK) {
        printf("[SD] f_readdir failed: %s (%d)\n", FRESULT_str(fr), fr);
        gameBoyROMCount = 0;
        if (result)
            *result = SDLoadResult::READ_FAILED;
        return 0;
    }

    if (result)
        *result = SDLoadResult::OK;
    printf("[SD] Found %u Game Boy ROM(s)\n",
           static_cast<unsigned>(gameBoyROMCount));
    return gameBoyROMCount;
}

size_t sdGameBoyROMCount() {
    return gameBoyROMCount;
}

const SDGameBoyROM *sdGameBoyROMAt(size_t index) {
    return index < gameBoyROMCount ? &gameBoyROMs[index] : nullptr;
}

const char *sdLoadResultText(SDLoadResult result) {
    switch (result) {
        case SDLoadResult::OK:
            return "OK";
        case SDLoadResult::CARD_NOT_READY:
            return "CARD NOT READY";
        case SDLoadResult::FILESYSTEM_UNSUPPORTED:
            return "FAT32 REQUIRED";
        case SDLoadResult::MOUNT_FAILED:
            return "MOUNT FAILED";
        case SDLoadResult::OPEN_FAILED:
            return "FILE NOT FOUND";
        case SDLoadResult::INVALID_SIZE:
            return "INVALID ROM";
        case SDLoadResult::OUT_OF_MEMORY:
            return "OUT OF MEMORY";
        case SDLoadResult::READ_FAILED:
            return "READ FAILED";
    }
    return "UNKNOWN ERROR";
}

uint8_t *sdLoadFile(const char *path, size_t *size, SDLoadResult *result) {
    if (size)
        *size = 0;
    if (result)
        *result = SDLoadResult::READ_FAILED;

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdFile, 0, sizeof(sdFile));
    bool fileOpen = false;
    uint8_t *buffer = nullptr;

    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        printf("[SD] f_mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        setMountFailure(fr, result);
        return nullptr;
    }

    fr = f_open(&sdFile, path, FA_READ);
    if (fr != FR_OK) {
        printf("[SD] f_open(%s) failed: %s (%d)\n", path,
               FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::OPEN_FAILED;
        f_unmount("");
        return nullptr;
    }
    fileOpen = true;

    const FSIZE_t rawSize = f_size(&sdFile);
    if (rawSize < MIN_GB_ROM_SIZE || rawSize > MAX_GB_ROM_SIZE ||
        (rawSize % (16 * 1024)) != 0) {
        printf("[SD] Invalid Game Boy ROM size: %llu\n",
               static_cast<unsigned long long>(rawSize));
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        f_close(&sdFile);
        f_unmount("");
        return nullptr;
    }

    buffer = static_cast<uint8_t *>(malloc(static_cast<size_t>(rawSize)));
    if (!buffer) {
        printf("[SD] Not enough memory for %llu-byte ROM\n",
               static_cast<unsigned long long>(rawSize));
        if (result)
            *result = SDLoadResult::OUT_OF_MEMORY;
        f_close(&sdFile);
        f_unmount("");
        return nullptr;
    }

    size_t total = 0;
    while (total < static_cast<size_t>(rawSize)) {
        const size_t remaining = static_cast<size_t>(rawSize) - total;
        const UINT request = static_cast<UINT>(remaining > 16384 ? 16384 : remaining);
        UINT bytesRead = 0;
        fr = f_read(&sdFile, buffer + total, request, &bytesRead);
        if (fr != FR_OK || bytesRead != request) {
            printf("[SD] f_read failed at %u: %s (%d), got %u/%u\n",
                   static_cast<unsigned>(total), FRESULT_str(fr), fr,
                   bytesRead, request);
            free(buffer);
            buffer = nullptr;
            if (result)
                *result = SDLoadResult::READ_FAILED;
            break;
        }
        total += bytesRead;
    }

    if (fileOpen)
        f_close(&sdFile);
    f_unmount("");

    if (!buffer)
        return nullptr;

    if (size)
        *size = total;
    if (result)
        *result = SDLoadResult::OK;
    printf("[SD] Loaded %s (%u bytes)\n", path, static_cast<unsigned>(total));
    return buffer;
}
