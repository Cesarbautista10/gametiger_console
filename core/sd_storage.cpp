#include "sd_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "flash_storage.h"
#include "hardware/flash.h"
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
constexpr size_t IO_CHUNK_SIZE = 16 * 1024;
constexpr size_t FLASH_COPY_CHUNK_SIZE = 4 * 1024;
constexpr size_t FLASH_ERASE_CHUNK_SIZE = 64 * 1024;
constexpr size_t SAVE_PATH_BUFFER_SIZE = 32;

static_assert((FLASH_ERASE_CHUNK_SIZE % FLASH_SECTOR_SIZE) == 0);

// Keep FatFs work objects out of the Pico SDK's small default stack.
FATFS sdFilesystem;
FIL sdFile;
DIR sdDirectory;
FILINFO sdFileInfo;
SDGameBoyROM gameBoyROMs[SD_MAX_GAME_BOY_ROMS];
size_t gameBoyROMCount = 0;
alignas(4) uint8_t flashCopyBuffer[FLASH_COPY_CHUNK_SIZE];

uint32_t updateCRC32(uint32_t crc, const uint8_t *data, size_t size) {
    while (size-- > 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

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

enum class SaveCandidateResult : uint8_t {
    MISSING,
    VALID,
    INVALID,
    IO_ERROR,
};

constexpr uint32_t SAVE_METADATA_MAGIC = 0x47545356; // "GTSV"
constexpr uint16_t SAVE_METADATA_VERSION = 1;

struct SaveMetadata {
    uint32_t magic;
    uint16_t version;
    uint16_t structureSize;
    uint32_t generation;
    uint32_t dataSize;
    uint32_t dataCRC32;
    uint32_t metadataCRC32;
};

static_assert(sizeof(SaveMetadata) == 24);

bool makeSaveSiblingPath(const char *path, const char *extension,
                         char output[SAVE_PATH_BUFFER_SIZE]) {
    if (!path || !extension || strlen(extension) != 3)
        return false;

    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash))
        dot = path + strlen(path);

    const size_t baseLength = static_cast<size_t>(dot - path);
    if (baseLength + 5 > SAVE_PATH_BUFFER_SIZE)
        return false;

    memcpy(output, path, baseLength);
    output[baseLength] = '.';
    memcpy(output + baseLength + 1, extension, 3);
    output[baseLength + 4] = '\0';
    return true;
}

uint32_t saveMetadataCRC32(const SaveMetadata &metadata) {
    return ~updateCRC32(
        0xFFFFFFFFU, reinterpret_cast<const uint8_t *>(&metadata),
        offsetof(SaveMetadata, metadataCRC32));
}

SaveCandidateResult readSaveData(const char *path, uint8_t *buffer,
                                 size_t size, uint32_t *crc32) {
    if (crc32)
        *crc32 = 0;
    memset(&sdFile, 0, sizeof(sdFile));
    FRESULT fr = f_open(&sdFile, path, FA_READ);
    if (fr == FR_NO_FILE || fr == FR_NO_PATH)
        return SaveCandidateResult::MISSING;
    if (fr != FR_OK) {
        printf("[SD] Save open %s failed: %s (%d)\n",
               path, FRESULT_str(fr), fr);
        return SaveCandidateResult::IO_ERROR;
    }

    if (static_cast<size_t>(f_size(&sdFile)) != size) {
        printf("[SD] Ignoring incomplete save %s (%llu/%u bytes)\n", path,
               static_cast<unsigned long long>(f_size(&sdFile)),
               static_cast<unsigned>(size));
        const FRESULT closeResult = f_close(&sdFile);
        return closeResult == FR_OK
            ? SaveCandidateResult::INVALID
            : SaveCandidateResult::IO_ERROR;
    }

    uint32_t runningCRC = 0xFFFFFFFFU;
    size_t total = 0;
    while (total < size) {
        const size_t remaining = size - total;
        const UINT request = static_cast<UINT>(
            remaining > (buffer ? IO_CHUNK_SIZE : FLASH_COPY_CHUNK_SIZE)
                ? (buffer ? IO_CHUNK_SIZE : FLASH_COPY_CHUNK_SIZE)
                : remaining);
        uint8_t *destination = buffer ? buffer + total : flashCopyBuffer;
        UINT bytesRead = 0;
        fr = f_read(&sdFile, destination, request, &bytesRead);
        if (fr != FR_OK || bytesRead != request)
            break;
        runningCRC = updateCRC32(runningCRC, destination, bytesRead);
        total += bytesRead;
    }
    const FRESULT closeResult = f_close(&sdFile);
    if (fr != FR_OK || closeResult != FR_OK || total != size) {
        const FRESULT failure = fr != FR_OK ? fr : closeResult;
        printf("[SD] Save read %s failed at %u: %s (%d)\n", path,
               static_cast<unsigned>(total), FRESULT_str(failure), failure);
        return SaveCandidateResult::IO_ERROR;
    }
    if (crc32)
        *crc32 = ~runningCRC;
    return SaveCandidateResult::VALID;
}

SaveCandidateResult readSaveMetadata(const char *path,
                                     SaveMetadata *metadata) {
    memset(metadata, 0, sizeof(*metadata));
    memset(&sdFile, 0, sizeof(sdFile));
    FRESULT fr = f_open(&sdFile, path, FA_READ);
    if (fr == FR_NO_FILE || fr == FR_NO_PATH)
        return SaveCandidateResult::MISSING;
    if (fr != FR_OK)
        return SaveCandidateResult::IO_ERROR;
    if (static_cast<size_t>(f_size(&sdFile)) != sizeof(*metadata)) {
        const FRESULT closeResult = f_close(&sdFile);
        return closeResult == FR_OK
            ? SaveCandidateResult::INVALID
            : SaveCandidateResult::IO_ERROR;
    }

    UINT bytesRead = 0;
    fr = f_read(&sdFile, metadata, sizeof(*metadata), &bytesRead);
    const FRESULT closeResult = f_close(&sdFile);
    if (fr != FR_OK || closeResult != FR_OK ||
        bytesRead != sizeof(*metadata)) {
        return SaveCandidateResult::IO_ERROR;
    }

    if (metadata->magic != SAVE_METADATA_MAGIC ||
        metadata->version != SAVE_METADATA_VERSION ||
        metadata->structureSize != sizeof(*metadata) ||
        metadata->metadataCRC32 != saveMetadataCRC32(*metadata)) {
        return SaveCandidateResult::INVALID;
    }
    return SaveCandidateResult::VALID;
}

bool writeSaveData(const char *path, const uint8_t *buffer, size_t size) {
    memset(&sdFile, 0, sizeof(sdFile));
    FRESULT fr = f_open(&sdFile, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("[SD] Save create %s failed: %s (%d)\n",
               path, FRESULT_str(fr), fr);
        return false;
    }

    size_t total = 0;
    while (total < size) {
        const size_t remaining = size - total;
        const UINT request = static_cast<UINT>(
            remaining > IO_CHUNK_SIZE ? IO_CHUNK_SIZE : remaining);
        UINT bytesWritten = 0;
        fr = f_write(&sdFile, buffer + total, request, &bytesWritten);
        if (fr != FR_OK || bytesWritten != request)
            break;
        total += bytesWritten;
    }
    if (fr == FR_OK && total == size)
        fr = f_sync(&sdFile);
    const FRESULT closeResult = f_close(&sdFile);
    if (fr != FR_OK || closeResult != FR_OK || total != size) {
        const FRESULT failure = fr != FR_OK ? fr : closeResult;
        printf("[SD] Save write %s failed at %u: %s (%d)\n", path,
               static_cast<unsigned>(total), FRESULT_str(failure), failure);
        return false;
    }
    return true;
}

bool writeSaveMetadata(const char *path, const SaveMetadata &metadata) {
    return writeSaveData(path,
                         reinterpret_cast<const uint8_t *>(&metadata),
                         sizeof(metadata));
}

struct SaveSlotState {
    const char *dataPath;
    const char *metadataPath;
    SaveMetadata metadata;
    SaveCandidateResult metadataResult;
    SaveCandidateResult dataResult;
    bool committed;
};

void inspectSaveSlot(SaveSlotState *slot, size_t expectedSize) {
    slot->metadataResult =
        readSaveMetadata(slot->metadataPath, &slot->metadata);
    slot->dataResult = SaveCandidateResult::MISSING;
    slot->committed = false;
    if (slot->metadataResult != SaveCandidateResult::VALID)
        return;
    if (slot->metadata.dataSize != expectedSize) {
        slot->metadataResult = SaveCandidateResult::INVALID;
        return;
    }

    uint32_t dataCRC = 0;
    slot->dataResult =
        readSaveData(slot->dataPath, nullptr, expectedSize, &dataCRC);
    slot->committed = slot->dataResult == SaveCandidateResult::VALID &&
        dataCRC == slot->metadata.dataCRC32;
    if (slot->dataResult == SaveCandidateResult::VALID && !slot->committed)
        slot->dataResult = SaveCandidateResult::INVALID;
}

bool generationIsNewer(uint32_t left, uint32_t right) {
    const uint32_t distance = left - right;
    return distance != 0 && distance < 0x80000000U;
}

SaveSlotState *newestCommittedSlot(SaveSlotState *first,
                                   SaveSlotState *second) {
    if (!first->committed)
        return second->committed ? second : nullptr;
    if (!second->committed)
        return first;
    return generationIsNewer(second->metadata.generation,
                             first->metadata.generation)
        ? second : first;
}

bool saveSlotHadIOError(const SaveSlotState &slot) {
    if (slot.metadataResult == SaveCandidateResult::IO_ERROR)
        return true;
    return slot.metadataResult == SaveCandidateResult::VALID &&
        slot.dataResult == SaveCandidateResult::IO_ERROR;
}

bool saveSlotWasDamaged(const SaveSlotState &slot) {
    return slot.metadataResult == SaveCandidateResult::INVALID ||
        (slot.metadataResult == SaveCandidateResult::VALID &&
         !slot.committed &&
         slot.dataResult != SaveCandidateResult::IO_ERROR);
}

bool initialiseSaveMetadata(SaveMetadata *metadata, uint32_t generation,
                            size_t size, uint32_t dataCRC32) {
    if (size > UINT32_MAX)
        return false;
    memset(metadata, 0, sizeof(*metadata));
    metadata->magic = SAVE_METADATA_MAGIC;
    metadata->version = SAVE_METADATA_VERSION;
    metadata->structureSize = sizeof(*metadata);
    metadata->generation = generation;
    metadata->dataSize = static_cast<uint32_t>(size);
    metadata->dataCRC32 = dataCRC32;
    metadata->metadataCRC32 = saveMetadataCRC32(*metadata);
    return true;
}
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
            sdFileInfo.fsize < SD_MIN_GAME_BOY_ROM_SIZE ||
            sdFileInfo.fsize > SD_MAX_GAME_BOY_ROM_SIZE ||
            (sdFileInfo.fsize % (16 * 1024)) != 0) {
            continue;
        }

        SDGameBoyROM entry = {};
        snprintf(entry.filename, sizeof(entry.filename), "%s",
                 sdFileInfo.fname);
        snprintf(entry.path, sizeof(entry.path), "/roms/%s",
                 sdFileInfo.fname);
        entry.size = static_cast<uint32_t>(sdFileInfo.fsize);
        entry.modifiedDate = sdFileInfo.fdate;
        entry.modifiedTime = sdFileInfo.ftime;
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
        case SDLoadResult::WRITE_FAILED:
            return "SAVE FAILED";
        case SDLoadResult::SAVE_DAMAGED:
            return "SAVE DAMAGED";
        case SDLoadResult::SAVE_RECOVERED:
            return "SAVE RECOVERED";
        case SDLoadResult::FLASH_CACHE_FAILED:
            return "ROM CACHE FAILED";
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
    if (rawSize < SD_MIN_GAME_BOY_ROM_SIZE ||
        rawSize > SD_MAX_GAME_BOY_ROM_SIZE ||
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
        const UINT request = static_cast<UINT>(
            remaining > IO_CHUNK_SIZE ? IO_CHUNK_SIZE : remaining);
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

bool sdReadFileRange(const char *path, size_t offset, uint8_t *buffer,
                     size_t length, size_t *fileSize, SDLoadResult *result) {
    if (fileSize)
        *fileSize = 0;
    if (result)
        *result = SDLoadResult::READ_FAILED;
    if (!path || (!buffer && length != 0)) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        return false;
    }

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdFile, 0, sizeof(sdFile));

    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        printf("[SD] Range mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        setMountFailure(fr, result);
        return false;
    }

    fr = f_open(&sdFile, path, FA_READ);
    if (fr != FR_OK) {
        printf("[SD] Range open %s failed: %s (%d)\n",
               path, FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::OPEN_FAILED;
        f_unmount("");
        return false;
    }

    const size_t rawSize = static_cast<size_t>(f_size(&sdFile));
    if (fileSize)
        *fileSize = rawSize;
    if (offset > rawSize || length > rawSize - offset) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        f_close(&sdFile);
        f_unmount("");
        return false;
    }

    fr = f_lseek(&sdFile, static_cast<FSIZE_t>(offset));
    size_t total = 0;
    while (fr == FR_OK && total < length) {
        const size_t remaining = length - total;
        const UINT request = static_cast<UINT>(
            remaining > IO_CHUNK_SIZE ? IO_CHUNK_SIZE : remaining);
        UINT bytesRead = 0;
        fr = f_read(&sdFile, buffer + total, request, &bytesRead);
        if (fr != FR_OK || bytesRead != request)
            break;
        total += bytesRead;
    }

    f_close(&sdFile);
    f_unmount("");
    if (fr != FR_OK || total != length) {
        printf("[SD] Range read %s failed at %u: %s (%d)\n", path,
               static_cast<unsigned>(offset + total), FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::READ_FAILED;
        return false;
    }

    if (result)
        *result = SDLoadResult::OK;
    return true;
}

bool sdCalculateFileCRC32(const char *path, size_t expectedSize,
                          uint32_t *crc32, SDLoadResult *result) {
    if (crc32)
        *crc32 = 0;
    if (result)
        *result = SDLoadResult::READ_FAILED;
    if (!path || !crc32 || expectedSize == 0) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        return false;
    }

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdFile, 0, sizeof(sdFile));

    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        printf("[SD] CRC mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        setMountFailure(fr, result);
        return false;
    }

    fr = f_open(&sdFile, path, FA_READ);
    if (fr != FR_OK) {
        printf("[SD] CRC open %s failed: %s (%d)\n",
               path, FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::OPEN_FAILED;
        f_unmount("");
        return false;
    }
    if (static_cast<size_t>(f_size(&sdFile)) != expectedSize) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        f_close(&sdFile);
        f_unmount("");
        return false;
    }

    uint32_t runningCRC = 0xFFFFFFFFU;
    size_t total = 0;
    while (total < expectedSize) {
        const size_t remaining = expectedSize - total;
        const UINT request = static_cast<UINT>(
            remaining > FLASH_COPY_CHUNK_SIZE
                ? FLASH_COPY_CHUNK_SIZE : remaining);
        UINT bytesRead = 0;
        fr = f_read(&sdFile, flashCopyBuffer, request, &bytesRead);
        if (fr != FR_OK || bytesRead != request)
            break;
        runningCRC = updateCRC32(runningCRC, flashCopyBuffer, request);
        total += bytesRead;
    }

    f_close(&sdFile);
    f_unmount("");
    if (fr != FR_OK || total != expectedSize) {
        printf("[SD] CRC read %s failed at %u: %s (%d)\n", path,
               static_cast<unsigned>(total), FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::READ_FAILED;
        return false;
    }

    *crc32 = ~runningCRC;
    if (result)
        *result = SDLoadResult::OK;
    return true;
}

bool sdStageFileInFlash(const char *path, size_t expectedSize,
                        uint32_t flashOffset, uint32_t *crc32,
                        SDLoadResult *result) {
    if (crc32)
        *crc32 = 0;
    if (result)
        *result = SDLoadResult::READ_FAILED;

    const size_t eraseSize =
        (expectedSize + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    if (!path || expectedSize < SD_MIN_GAME_BOY_ROM_SIZE ||
        expectedSize > SD_MAX_GAME_BOY_ROM_SIZE ||
        (expectedSize % FLASH_PAGE_SIZE) != 0 ||
        (flashOffset % FLASH_SECTOR_SIZE) != 0 ||
        flashOffset > PICO_FLASH_SIZE_BYTES ||
        eraseSize > PICO_FLASH_SIZE_BYTES - flashOffset) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        return false;
    }

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdFile, 0, sizeof(sdFile));

    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        printf("[SD] Cache mount failed: %s (%d)\n", FRESULT_str(fr), fr);
        setMountFailure(fr, result);
        return false;
    }

    fr = f_open(&sdFile, path, FA_READ);
    if (fr != FR_OK) {
        printf("[SD] Cache open %s failed: %s (%d)\n",
               path, FRESULT_str(fr), fr);
        if (result)
            *result = SDLoadResult::OPEN_FAILED;
        f_unmount("");
        return false;
    }
    if (static_cast<size_t>(f_size(&sdFile)) != expectedSize) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        f_close(&sdFile);
        f_unmount("");
        return false;
    }

    printf("[SD] Staging %s (%u bytes) in XIP flash\n", path,
           static_cast<unsigned>(expectedSize));
    for (size_t erased = 0; erased < eraseSize;) {
        const size_t remaining = eraseSize - erased;
        const size_t chunk = remaining > FLASH_ERASE_CHUNK_SIZE
            ? FLASH_ERASE_CHUNK_SIZE : remaining;
        if (!flashStorageErase(
                flashOffset + static_cast<uint32_t>(erased), chunk)) {
            printf("[SD] XIP cache erase failed at %u\n",
                   static_cast<unsigned>(erased));
            if (result)
                *result = SDLoadResult::FLASH_CACHE_FAILED;
            f_close(&sdFile);
            f_unmount("");
            return false;
        }
        erased += chunk;
    }

    uint32_t runningCRC = 0xFFFFFFFFU;
    size_t total = 0;
    bool success = true;
    while (total < expectedSize) {
        const size_t remaining = expectedSize - total;
        const UINT request = static_cast<UINT>(
            remaining > FLASH_COPY_CHUNK_SIZE
                ? FLASH_COPY_CHUNK_SIZE : remaining);
        UINT bytesRead = 0;
        fr = f_read(&sdFile, flashCopyBuffer, request, &bytesRead);
        if (fr != FR_OK || bytesRead != request) {
            printf("[SD] Cache read failed at %u: %s (%d), got %u/%u\n",
                   static_cast<unsigned>(total), FRESULT_str(fr), fr,
                   bytesRead, request);
            if (result)
                *result = SDLoadResult::READ_FAILED;
            success = false;
            break;
        }

        runningCRC = updateCRC32(runningCRC, flashCopyBuffer, request);
        if (!flashStorageProgram(
                flashOffset + static_cast<uint32_t>(total),
                flashCopyBuffer, request)) {
            printf("[SD] XIP cache program failed at %u\n",
                   static_cast<unsigned>(total));
            if (result)
                *result = SDLoadResult::FLASH_CACHE_FAILED;
            success = false;
            break;
        }

        const uint8_t *programmed = reinterpret_cast<const uint8_t *>(
            XIP_BASE + flashOffset + static_cast<uint32_t>(total));
        if (memcmp(programmed, flashCopyBuffer, request) != 0) {
            printf("[SD] XIP verification failed at %u\n",
                   static_cast<unsigned>(total));
            if (result)
                *result = SDLoadResult::FLASH_CACHE_FAILED;
            success = false;
            break;
        }
        total += request;
    }

    f_close(&sdFile);
    f_unmount("");
    if (!success)
        return false;

    if (crc32)
        *crc32 = ~runningCRC;
    if (result)
        *result = SDLoadResult::OK;
    printf("[SD] XIP cache ready, CRC32=%08lX\n",
           static_cast<unsigned long>(~runningCRC));
    return true;
}

bool sdLoadSaveFile(const char *path, uint8_t *buffer, size_t size,
                    bool *found, SDLoadResult *result) {
    if (found)
        *found = false;
    if (result)
        *result = SDLoadResult::READ_FAILED;
    char backupPath[SAVE_PATH_BUFFER_SIZE];
    char primaryMetadataPath[SAVE_PATH_BUFFER_SIZE];
    char backupMetadataPath[SAVE_PATH_BUFFER_SIZE];
    if (!path || !buffer || size == 0 ||
        !makeSaveSiblingPath(path, "BAK", backupPath) ||
        !makeSaveSiblingPath(path, "SV0", primaryMetadataPath) ||
        !makeSaveSiblingPath(path, "SV1", backupMetadataPath)) {
        if (result)
            *result = SDLoadResult::INVALID_SIZE;
        return false;
    }

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        setMountFailure(fr, result);
        return false;
    }

    SaveSlotState primary = {
        path, primaryMetadataPath, {}, SaveCandidateResult::MISSING,
        SaveCandidateResult::MISSING, false
    };
    SaveSlotState backup = {
        backupPath, backupMetadataPath, {}, SaveCandidateResult::MISSING,
        SaveCandidateResult::MISSING, false
    };
    inspectSaveSlot(&primary, size);
    inspectSaveSlot(&backup, size);

    if (saveSlotHadIOError(primary) || saveSlotHadIOError(backup)) {
        f_unmount("");
        if (result)
            *result = SDLoadResult::READ_FAILED;
        return false;
    }

    SaveSlotState *active = newestCommittedSlot(&primary, &backup);
    if (active) {
        uint32_t loadedCRC = 0;
        const SaveCandidateResult loadResult =
            readSaveData(active->dataPath, buffer, size, &loadedCRC);
        if (loadResult != SaveCandidateResult::VALID ||
            loadedCRC != active->metadata.dataCRC32) {
            f_unmount("");
            memset(buffer, 0, size);
            if (result)
                *result = SDLoadResult::READ_FAILED;
            return false;
        }

        const SaveSlotState *other = active == &primary ? &backup : &primary;
        bool recoveredOlderGeneration = saveSlotWasDamaged(*other);
        if (!recoveredOlderGeneration && !other->committed &&
            other->metadataResult == SaveCandidateResult::VALID &&
            generationIsNewer(other->metadata.generation,
                              active->metadata.generation)) {
            recoveredOlderGeneration = true;
        }

        f_unmount("");
        if (found)
            *found = true;
        if (result)
            *result = recoveredOlderGeneration
                ? SDLoadResult::SAVE_RECOVERED : SDLoadResult::OK;
        printf("[SD] Loaded save %s generation %lu (%u bytes)\n",
               active->dataPath,
               static_cast<unsigned long>(active->metadata.generation),
               static_cast<unsigned>(size));
        return true;
    }

    const bool metadataMissing =
        primary.metadataResult == SaveCandidateResult::MISSING &&
        backup.metadataResult == SaveCandidateResult::MISSING;
    if (primary.metadataResult == SaveCandidateResult::MISSING) {
        // A standard raw .SAV is the only legacy/import format. Uncommitted
        // .BAK data is deliberately ignored: it may be a write that lost
        // power before its CRC sidecar was published. The raw .SAV remains a
        // valid import even if an interrupted first commit damaged SV1.
        uint32_t ignoredCRC = 0;
        const SaveCandidateResult legacy =
            readSaveData(path, buffer, size, &ignoredCRC);
        if (legacy == SaveCandidateResult::IO_ERROR) {
            f_unmount("");
            memset(buffer, 0, size);
            if (result)
                *result = SDLoadResult::READ_FAILED;
            return false;
        }
        if (legacy == SaveCandidateResult::VALID) {
            f_unmount("");
            if (found)
                *found = true;
            if (result)
                *result = saveSlotWasDamaged(backup)
                    ? SDLoadResult::SAVE_RECOVERED : SDLoadResult::OK;
            printf("[SD] Imported legacy save %s (%u bytes)\n",
                   path, static_cast<unsigned>(size));
            return true;
        }
        if (legacy == SaveCandidateResult::INVALID) {
            f_unmount("");
            memset(buffer, 0, size);
            if (result)
                *result = SDLoadResult::SAVE_DAMAGED;
            return true;
        }
    }

    const bool damaged = saveSlotWasDamaged(primary) ||
        saveSlotWasDamaged(backup) || !metadataMissing;
    f_unmount("");
    memset(buffer, 0, size);
    if (result)
        *result = damaged ? SDLoadResult::SAVE_DAMAGED : SDLoadResult::OK;
    return true;
}

bool sdWriteSaveFile(const char *path, const uint8_t *buffer, size_t size,
                     SDLoadResult *result) {
    if (result)
        *result = SDLoadResult::WRITE_FAILED;
    char backupPath[SAVE_PATH_BUFFER_SIZE];
    char primaryMetadataPath[SAVE_PATH_BUFFER_SIZE];
    char backupMetadataPath[SAVE_PATH_BUFFER_SIZE];
    if (!path || !buffer || size == 0 ||
        !makeSaveSiblingPath(path, "BAK", backupPath) ||
        !makeSaveSiblingPath(path, "SV0", primaryMetadataPath) ||
        !makeSaveSiblingPath(path, "SV1", backupMetadataPath)) {
        return false;
    }

    DisplaySPIStateGuard spiGuard;
    memset(&sdFilesystem, 0, sizeof(sdFilesystem));
    memset(&sdFile, 0, sizeof(sdFile));
    FRESULT fr = f_mount(&sdFilesystem, "", 1);
    if (fr != FR_OK) {
        setMountFailure(fr, result);
        return false;
    }

    fr = f_mkdir("/saves");
    if (fr != FR_OK && fr != FR_EXIST) {
        printf("[SD] Save directory creation failed: %s (%d)\n",
               FRESULT_str(fr), fr);
        f_unmount("");
        return false;
    }

    SaveSlotState primary = {
        path, primaryMetadataPath, {}, SaveCandidateResult::MISSING,
        SaveCandidateResult::MISSING, false
    };
    SaveSlotState backup = {
        backupPath, backupMetadataPath, {}, SaveCandidateResult::MISSING,
        SaveCandidateResult::MISSING, false
    };
    inspectSaveSlot(&primary, size);
    inspectSaveSlot(&backup, size);
    if (saveSlotHadIOError(primary) || saveSlotHadIOError(backup)) {
        printf("[SD] Cannot safely choose a save slot after an I/O error\n");
        f_unmount("");
        return false;
    }

    SaveSlotState *active = newestCommittedSlot(&primary, &backup);
    SaveSlotState *target = active == &primary ? &backup : &primary;
    if (!active)
        target = &backup; // Preserve/import .SAV; uncommitted BAK is ignored.

    const uint32_t generation = active
        ? active->metadata.generation + 1U : 1U;
    const uint32_t dataCRC =
        ~updateCRC32(0xFFFFFFFFU, buffer, size);
    SaveMetadata metadata;
    if (!initialiseSaveMetadata(&metadata, generation, size, dataCRC) ||
        !writeSaveData(target->dataPath, buffer, size) ||
        !writeSaveMetadata(target->metadataPath, metadata)) {
        f_unmount("");
        return false;
    }

    // Verify the newly committed slot before allowing the caller to clear
    // its dirty flag. The other slot was never modified.
    inspectSaveSlot(target, size);
    if (!target->committed ||
        target->metadata.generation != generation ||
        target->metadata.dataCRC32 != dataCRC) {
        printf("[SD] Save verification failed for %s\n", target->dataPath);
        f_unmount("");
        return false;
    }

    f_unmount("");

    if (result)
        *result = SDLoadResult::OK;
    printf("[SD] Saved %s generation %lu (%u bytes)\n",
           target->dataPath, static_cast<unsigned long>(generation),
           static_cast<unsigned>(size));
    return true;
}
