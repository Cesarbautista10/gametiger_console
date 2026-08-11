#include "gameboyscreen.h"
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "../core/Peanut-GB/peanut_gb.h"
#include "../core/flash_storage.h"
#include "../core/sd_storage.h"
#include "hardware/flash.h"

extern "C" char __flash_binary_end;

namespace {
constexpr uint64_t GAME_BOY_FRAME_US = 16743;
constexpr uint64_t MAX_CATCH_UP_FRAMES = 4;
constexpr uint64_t SYSTEM_PAUSE_HOLD_US = 800000;
constexpr uint64_t AUTO_SAVE_IDLE_US = 500000;
constexpr uint64_t AUTO_SAVE_MAX_DELAY_US = 5000000;
constexpr uint64_t SAVE_RETRY_US = 5000000;
constexpr size_t ROM_HEADER_SIZE = 0x150;
constexpr size_t MAX_RESIDENT_ROM_SIZE = 256 * 1024;

// The Pico 2 has 4 MiB of XIP flash. The application currently ends below
// 1 MiB and high scores occupy 0x180000, so keep the large-ROM cache in a
// separate 1 MiB slot. Metadata lives in the following sector and is written
// only after the complete ROM has been verified.
constexpr uint32_t ROM_FLASH_CACHE_OFFSET = 0x200000;
constexpr size_t ROM_FLASH_CACHE_SIZE = 1024 * 1024;
constexpr uint32_t ROM_FLASH_METADATA_OFFSET = 0x300000;
constexpr uint32_t ROM_FLASH_CACHE_MAGIC = 0x47425243; // "GBRC"
constexpr uint16_t ROM_FLASH_CACHE_VERSION = 1;

static_assert(ROM_FLASH_CACHE_OFFSET + ROM_FLASH_CACHE_SIZE <=
              ROM_FLASH_METADATA_OFFSET);
static_assert(ROM_FLASH_METADATA_OFFSET + FLASH_SECTOR_SIZE <=
              PICO_FLASH_SIZE_BYTES);

struct ROMFlashCacheMetadata {
    uint32_t magic;
    uint16_t version;
    uint16_t structureSize;
    uint32_t romSize;
    uint32_t romCRC32;
    uint16_t modifiedDate;
    uint16_t modifiedTime;
    uint16_t globalChecksum;
    uint8_t headerChecksum;
    uint8_t cartridgeType;
    uint8_t romSizeCode;
    uint8_t reserved;
    char path[SD_GAME_BOY_PATH_SIZE];
    uint32_t metadataChecksum;
};

static_assert(sizeof(ROMFlashCacheMetadata) <= FLASH_PAGE_SIZE);

alignas(4) uint8_t flashMetadataPage[FLASH_PAGE_SIZE];
alignas(4) uint8_t romHeader[ROM_HEADER_SIZE];

uint32_t updateCRC32(uint32_t crc, const uint8_t *data, size_t size) {
    while (size-- > 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

uint32_t calculateCRC32(const uint8_t *data, size_t size) {
    return ~updateCRC32(0xFFFFFFFFU, data, size);
}

uint32_t calculateMetadataChecksum(const ROMFlashCacheMetadata &metadata) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&metadata);
    uint32_t hash = 2166136261U;
    for (size_t index = 0;
         index < offsetof(ROMFlashCacheMetadata, metadataChecksum); ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

bool cacheIdentityMatches(const ROMFlashCacheMetadata &metadata,
                          const SDGameBoyROM &rom,
                          const uint8_t header[ROM_HEADER_SIZE]) {
    return metadata.magic == ROM_FLASH_CACHE_MAGIC &&
        metadata.version == ROM_FLASH_CACHE_VERSION &&
        metadata.structureSize == sizeof(ROMFlashCacheMetadata) &&
        metadata.metadataChecksum == calculateMetadataChecksum(metadata) &&
        metadata.romSize == rom.size &&
        metadata.modifiedDate == rom.modifiedDate &&
        metadata.modifiedTime == rom.modifiedTime &&
        metadata.globalChecksum ==
            (static_cast<uint16_t>(header[0x14E]) << 8 | header[0x14F]) &&
        metadata.headerChecksum == header[0x14D] &&
        metadata.cartridgeType == header[0x147] &&
        metadata.romSizeCode == header[0x148] &&
        strncmp(metadata.path, rom.path, sizeof(metadata.path)) == 0;
}

bool writeCacheMetadata(const ROMFlashCacheMetadata &metadata) {
    memset(flashMetadataPage, 0xFF, sizeof(flashMetadataPage));
    memcpy(flashMetadataPage, &metadata, sizeof(metadata));
    if (!flashStorageProgram(ROM_FLASH_METADATA_OFFSET,
                             flashMetadataPage, sizeof(flashMetadataPage))) {
        return false;
    }
    return memcmp(reinterpret_cast<const void *>(
                      XIP_BASE + ROM_FLASH_METADATA_OFFSET),
                  flashMetadataPage, sizeof(flashMetadataPage)) == 0;
}

bool prepareFlashCachedROM(const SDGameBoyROM &rom,
                           const uint8_t header[ROM_HEADER_SIZE],
                           SDLoadResult *result) {
    const uintptr_t binaryEndOffset =
        reinterpret_cast<uintptr_t>(&__flash_binary_end) - XIP_BASE;
    if (binaryEndOffset > ROM_FLASH_CACHE_OFFSET ||
        rom.size > ROM_FLASH_CACHE_SIZE) {
        printf("[GameBoyScreen] Flash layout cannot cache this ROM\n");
        if (result)
            *result = SDLoadResult::FLASH_CACHE_FAILED;
        return false;
    }

    ROMFlashCacheMetadata metadata;
    memcpy(&metadata, reinterpret_cast<const void *>(
               XIP_BASE + ROM_FLASH_METADATA_OFFSET), sizeof(metadata));
    const uint8_t *cachedROM = reinterpret_cast<const uint8_t *>(
        XIP_BASE + ROM_FLASH_CACHE_OFFSET);

    if (cacheIdentityMatches(metadata, rom, header)) {
        const uint32_t cachedCRC = calculateCRC32(cachedROM, rom.size);
        if (cachedCRC == metadata.romCRC32) {
            uint32_t sourceCRC = 0;
            if (!sdCalculateFileCRC32(rom.path, rom.size, &sourceCRC,
                                      result)) {
                return false;
            }
            if (sourceCRC == cachedCRC) {
                printf("[GameBoyScreen] Reusing XIP ROM cache, CRC32=%08lX\n",
                       static_cast<unsigned long>(cachedCRC));
                if (result)
                    *result = SDLoadResult::OK;
                return true;
            }
            printf("[GameBoyScreen] SD ROM changed; rebuilding cache\n");
        } else {
            printf("[GameBoyScreen] Cached ROM CRC mismatch; rebuilding\n");
        }
    }

    // Invalidate first so a power loss can never make a partial copy valid.
    if (!flashStorageErase(ROM_FLASH_METADATA_OFFSET, FLASH_SECTOR_SIZE)) {
        if (result)
            *result = SDLoadResult::FLASH_CACHE_FAILED;
        return false;
    }

    uint32_t sourceCRC = 0;
    if (!sdStageFileInFlash(rom.path, rom.size, ROM_FLASH_CACHE_OFFSET,
                            &sourceCRC, result)) {
        return false;
    }
    const uint32_t cachedCRC = calculateCRC32(cachedROM, rom.size);
    if (sourceCRC != cachedCRC) {
        printf("[GameBoyScreen] Whole-ROM cache CRC mismatch\n");
        if (result)
            *result = SDLoadResult::FLASH_CACHE_FAILED;
        return false;
    }

    memset(&metadata, 0, sizeof(metadata));
    metadata.magic = ROM_FLASH_CACHE_MAGIC;
    metadata.version = ROM_FLASH_CACHE_VERSION;
    metadata.structureSize = sizeof(ROMFlashCacheMetadata);
    metadata.romSize = rom.size;
    metadata.romCRC32 = cachedCRC;
    metadata.modifiedDate = rom.modifiedDate;
    metadata.modifiedTime = rom.modifiedTime;
    metadata.globalChecksum =
        static_cast<uint16_t>(header[0x14E]) << 8 | header[0x14F];
    metadata.headerChecksum = header[0x14D];
    metadata.cartridgeType = header[0x147];
    metadata.romSizeCode = header[0x148];
    snprintf(metadata.path, sizeof(metadata.path), "%s", rom.path);
    metadata.metadataChecksum = calculateMetadataChecksum(metadata);
    if (!writeCacheMetadata(metadata)) {
        if (result)
            *result = SDLoadResult::FLASH_CACHE_FAILED;
        return false;
    }

    if (result)
        *result = SDLoadResult::OK;
    return true;
}

std::string savePathForROM(const char *filename) {
    char basename[9] = {};
    size_t length = 0;
    while (filename[length] != '\0' && filename[length] != '.' &&
           length < sizeof(basename) - 1) {
        basename[length] = filename[length];
        ++length;
    }
    if (length == 0)
        snprintf(basename, sizeof(basename), "GAME");
    return std::string("/saves/") + basename + ".sav";
}

uint8_t joypadMaskForKey(uint8_t key) {
    switch (key) {
        case KEY_A:     return 1U << 0;
        case KEY_B:     return 1U << 1;
        case KEY_SELECT:return 1U << 2;
        case KEY_START: return 1U << 3;
        case KEY_RIGHT: return 1U << 4;
        case KEY_LEFT:  return 1U << 5;
        case KEY_UP:    return 1U << 6;
        case KEY_DOWN:  return 1U << 7;
        default:        return 0;
    }
}

bool validateGameBoyROM(const uint8_t *header, size_t size,
                        const char **error) {
    if (!header || size < ROM_HEADER_SIZE) {
        *error = "INVALID GB ROM";
        return false;
    }

    // Peanut-GB indexes fixed tables with these header bytes. Validate them
    // before gb_init() so a malformed/truncated file cannot index out of bounds.
    const uint8_t romSizeCode = header[0x148];
    const uint8_t ramSizeCode = header[0x149];
    if (romSizeCode > 8 || ramSizeCode > 4) {
        *error = "UNSUPPORTED ROM";
        return false;
    }

    const size_t declaredSize = static_cast<size_t>(32 * 1024) << romSizeCode;
    if (declaredSize != size) {
        *error = "ROM SIZE MISMATCH";
        return false;
    }

    // Peanut-GB emulates the original monochrome Game Boy, not CGB-only ROMs.
    if (header[0x143] == 0xC0) {
        *error = "GBC NOT SUPPORTED";
        return false;
    }

    uint8_t checksum = 0;
    for (size_t index = 0x134; index <= 0x14C; ++index)
        checksum = checksum - header[index] - 1;
    if (checksum != header[0x14D]) {
        *error = "BAD ROM CHECKSUM";
        return false;
    }

    return true;
}
}

void gb_load_palette(struct gb_s *gb) {
    GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;

    uint8_t checksum = gb_colour_hash(gb);
    printf("Game Hash: %X\n", checksum);
    p->palette = new Color[3 * 4];

    switch(checksum) {
    	/* Balloon Kid (USA, Europe) */
        /* Tetris Blast (USA, Europe) */
        case 0x71:
        case 0xFF: {
            const uint32_t palette[3][4] = {
                { 0xFFFFFF, 0xFF9C00, 0xFF0000, 0x000000 }, /* OBJ0 */
                { 0xFFFFFF, 0xFF9C00, 0xFF0000, 0x000000 }, /* OBJ1 */
                { 0xFFFFFF, 0xFF9C00, 0xFF0000, 0x000000 }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        /* Hoshi no Kirby */
        /* Kirby no Block Ball */
        /* Kirby's Block Ball */
        /* Kirby's Dream Land */
        case 0x27:
        case 0x49:
        case 0x5C:
        case 0xB3: {
            const uint32_t palette[3][4] = {
                { 0xFF6352, 0xD60000, 0x630000, 0x000000 }, /* OBJ0 */
                { 0x0000FF, 0xFFFFFF, 0xFFFF7B, 0x0084FF }, /* OBJ1 */
                { 0xA59CFF, 0xFFFF00, 0x006300, 0x000000 }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        /* Qix */
        /* Tetris 2 */
        /* Tetris Flash */
        case 0x0D:
        case 0x69:
        case 0xF2: {
            const uint32_t palette[3][4] = {
                { 0xFFFFFF, 0xFFFF00, 0xFF0000, 0x000000 }, /* OBJ0 */
                { 0xFFFFFF, 0xFFFF00, 0xFF0000, 0x000000 }, /* OBJ1 */
                { 0xFFFFFF, 0x5ABDFF, 0xFF0000, 0x0000FF }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        /* Pocket Monsters - Pikachu */
        /* Tetris */
        case 0x15:
        case 0xDB: {
            const uint32_t palette[3][4] = {
                { 0xFFFFFF, 0xFFFF00, 0xFF0000, 0x000000 }, /* OBJ0 */
                { 0xFFFFFF, 0xFFFF00, 0xFF0000, 0x000000 }, /* OBJ1 */
                { 0xFFFFFF, 0xFFFF00, 0xFF0000, 0x000000 }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        /* Super Mario Land 2 */
        case 0xC9: {
            const uint32_t palette[3][4] = {
                { 0xFFFFFF, 0xFF7300, 0x944200, 0x000000 }, /* OBJ0 */
                { 0xFFFFFF, 0x63A5FF, 0x0000FF, 0x000000 }, /* OBJ1 */
                { 0xFFFFCE, 0x63EFEF, 0x9C8431, 0x5A5A5A }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        /* Metroid II - Return of Samus  */
        case 0x46: {
            const uint32_t palette[3][4] = {
                { 0xFFFF00, 0xFF0000, 0x630000, 0x000000 }, /* OBJ0 */
                { 0xFFFFFF, 0x7BFF31, 0x008400, 0x000000 }, /* OBJ1 */
                { 0xFFFFFF, 0x63A5FF, 0x0000FF, 0x000000 }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }

        default: {
            const uint32_t palette[3][4] = {
                { 0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F }, /* OBJ0 */
                { 0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F }, /* OBJ1 */
                { 0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F }  /* BG */
            };
            for(uint8_t obj = 0; obj < 3; obj++)
                for(uint8_t layer = 0; layer < 4; layer++)
                    p->palette[(obj * 4) + layer] = Color(palette[obj][layer]);
            break;
        }
    }
}

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;
    if (!p->rom || addr >= p->rom_size)
        return 0xFF;
    return p->rom[addr];
}

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;
    if (!p->cart_ram || addr >= p->cart_ram_size)
        return 0xFF;
    return p->cart_ram[addr];
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;
    if (!p->cart_ram || addr >= p->cart_ram_size)
        return;
    if (p->cart_ram[addr] != val) {
        const uint64_t now = time_us_64();
        p->cart_ram[addr] = val;
        if (!p->cart_ram_dirty)
            p->cart_ram_dirty_since_us = now;
        p->cart_ram_dirty = true;
        p->cart_ram_last_write_us = now;
    }
}

[[noreturn]] void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t addr) {
	const char* gb_err_str[GB_INVALID_MAX] = {
		"UNKNOWN",
		"INVALID OPCODE",
		"INVALID READ",
		"INVALID WRITE",
		"HALT FOREVER"
	};
    GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;
	const char *message = gb_err < GB_INVALID_MAX ? gb_err_str[gb_err] : "UNKNOWN";
	printf("[GameBoyScreen] Error %d: %s at %04X\n", gb_err, message, addr);
	p->emulator_error = true;
	p->error_message = "EMULATOR ERROR";
	// Peanut-GB documents this callback as non-returning. Returning would reach
	// __builtin_unreachable() inside the core and can corrupt state or hardfault.
	panic("Peanut-GB error %d at %04X", gb_err, addr);
}

void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line) {
	GameBoyScreen* p = (GameBoyScreen*)gb->direct.priv;
    constexpr uint16_t sourceWidth = 160;
    constexpr uint16_t sourceHeight = 144;
    constexpr uint16_t pixelWidth = 267;
    constexpr uint16_t xOffset = (DISPLAY_WIDTH - pixelWidth) / 2;

    if(!p || !p->display || line >= sourceHeight) {
        return;
    }

    // Scale 160x144 to 267x240 while preserving the original aspect ratio.
    // The previous inclusive end row wrote 382 rows per frame and x=267 read
    // pixels[160], one byte beyond the source scanline.
    const uint16_t firstRow = line * DISPLAY_HEIGHT / sourceHeight;
    const uint16_t endRow = (line + 1) * DISPLAY_HEIGHT / sourceHeight;

    // The Pico SDK configures a 2 KiB core-0 stack. Keep this scanline buffer
    // in static storage instead of spending 534 bytes per callback.
    static std::array<Color, pixelWidth> lineColor;
    for(uint16_t x = 0; x < pixelWidth; x++) {
        const uint16_t sourceX = x * sourceWidth / pixelWidth;
        const uint8_t obj = (pixels[sourceX] & 0x30) >> 4;
        const uint8_t layer = pixels[sourceX] & 3;
        lineColor[x] = p->palette[(obj * 4) + layer];
    }

    for(uint16_t y = firstRow; y < endRow; y++)
        p->display->drawBitmapRow(Vec2(xOffset, y), pixelWidth, lineColor.data());
}

GameBoyScreen::GameBoyScreen(
        void (*rcb)(int8_t menu, uint8_t option),
        void (*hscb)(uint32_t highscore), uint32_t highscore,
        uint8_t option) {
    printf("[GameBoyScreen] loading...\n");
    this->screenId = ScreenEnum::GAMEBOYSCREEN;
    this->returnCallBack = rcb;
    this->highScoreCallBack = hscb;
    this->option = option;
    this->rom = nullptr;
    this->rom_size = 0;
    this->rom_owned = false;
    this->cart_ram = nullptr;
    this->cart_ram_size = 0;
    this->gb_ptr = nullptr;
    this->display = NULL;
    this->palette = nullptr;
    this->load_failed = false;
    this->emulator_error = false;
    this->lastFrameTimeUS = 0;
    this->frameAccumulatorUS = 0;
    this->exitRequested = false;
    this->paused = false;
    this->pendingJoypadPresses = 0;
    this->pendingJoypadReleases = 0;
    this->selectPressedAtUS = 0;
    this->selectHoldTriggered = false;
    this->resumeAPressed = false;
    this->saveRequested = false;
    this->saveFailed = false;
    this->saveWarning = false;
    this->saveWasReset = false;
    this->exitAfterSave = false;
    this->nextSaveAttemptUS = 0;
    this->cart_ram_dirty = false;
    this->cart_ram_last_write_us = 0;
    this->cart_ram_dirty_since_us = 0;
    this->romPath = "/roms";
    this->savePath.clear();

    SDLoadResult loadResult = SDLoadResult::READ_FAILED;
    if (option == 0) {
        this->load_failed = true;
        this->error_message = "ROM NOT FOUND";
        return;
    }
    const size_t selectedIndex = option - 1;
    const SDGameBoyROM *selectedROM = sdGameBoyROMAt(selectedIndex);
    if (!selectedROM) {
        sdScanGameBoyROMs(&loadResult);
        selectedROM = sdGameBoyROMAt(selectedIndex);
    }
    if (!selectedROM) {
        this->load_failed = true;
        this->error_message = loadResult == SDLoadResult::OK
            ? "ROM NOT FOUND"
            : sdLoadResultText(loadResult);
        printf("[GameBoyScreen] ROM option %u is not available\n", option);
        return;
    }

    const SDGameBoyROM romEntry = *selectedROM;
    this->romPath = romEntry.path;
    this->savePath = savePathForROM(romEntry.filename);
    const char *validationError = nullptr;
    if (romEntry.size <= MAX_RESIDENT_ROM_SIZE) {
        this->rom = sdLoadFile(this->romPath.c_str(), &this->rom_size,
                               &loadResult);
        if (!this->rom) {
            this->load_failed = true;
            this->error_message = sdLoadResultText(loadResult);
            printf("[GameBoyScreen] ROM load failed: %s\n",
                   this->error_message.c_str());
            return;
        }
        this->rom_owned = true;
        if (!validateGameBoyROM(this->rom, this->rom_size,
                                &validationError)) {
            this->load_failed = true;
            this->error_message = validationError;
            free(this->rom);
            this->rom = nullptr;
            this->rom_size = 0;
            this->rom_owned = false;
            return;
        }
    } else {
        size_t fileSize = 0;
        if (!sdReadFileRange(this->romPath.c_str(), 0, romHeader,
                             sizeof(romHeader), &fileSize, &loadResult)) {
            this->load_failed = true;
            this->error_message = sdLoadResultText(loadResult);
            return;
        }
        if (fileSize != romEntry.size ||
            !validateGameBoyROM(romHeader, fileSize, &validationError)) {
            this->load_failed = true;
            this->error_message = validationError
                ? validationError : "ROM SIZE CHANGED";
            return;
        }
        if (!prepareFlashCachedROM(romEntry, romHeader, &loadResult)) {
            this->load_failed = true;
            this->error_message = sdLoadResultText(loadResult);
            return;
        }
        this->rom = reinterpret_cast<uint8_t *>(
            XIP_BASE + ROM_FLASH_CACHE_OFFSET);
        this->rom_size = fileSize;
        this->rom_owned = false;
    }

    this->gb_ptr = new gb_s();

    enum gb_init_error_e ret = gb_init(
        static_cast<gb_s *>(this->gb_ptr), &gb_rom_read, &gb_cart_ram_read,
        &gb_cart_ram_write, &gb_error, this);
    if (ret != GB_INIT_NO_ERROR) {
        printf("[GameBoyScreen] gb_init failed: %d\n", ret);
        delete static_cast<gb_s *>(this->gb_ptr);
        this->gb_ptr = nullptr;
        this->load_failed = true;
        this->error_message = "INVALID GB ROM";
        if (this->rom_owned)
            free(this->rom);
        this->rom = nullptr;
        this->rom_size = 0;
        this->rom_owned = false;
        return;
    }

    if (gb_get_save_size_s(static_cast<gb_s *>(this->gb_ptr),
                           &this->cart_ram_size) != 0) {
        this->load_failed = true;
        this->error_message = "INVALID SAVE SIZE";
        delete static_cast<gb_s *>(this->gb_ptr);
        this->gb_ptr = nullptr;
        if (this->rom_owned)
            free(this->rom);
        this->rom = nullptr;
        this->rom_size = 0;
        this->rom_owned = false;
        return;
    }
    printf("[GameBoyScreen] cart_ram_size: %u\n",
           static_cast<unsigned>(this->cart_ram_size));
    if (this->cart_ram_size > 0) {
        this->cart_ram = static_cast<uint8_t *>(
            calloc(this->cart_ram_size, 1));
        if (!this->cart_ram) {
            this->load_failed = true;
            this->error_message = "NO SAVE MEMORY";
            delete static_cast<gb_s *>(this->gb_ptr);
            this->gb_ptr = nullptr;
            if (this->rom_owned)
                free(this->rom);
            this->rom = nullptr;
            this->rom_size = 0;
            this->rom_owned = false;
            return;
        }

        bool saveFound = false;
        if (!sdLoadSaveFile(this->savePath.c_str(), this->cart_ram,
                            this->cart_ram_size, &saveFound, &loadResult)) {
            this->load_failed = true;
            this->error_message = sdLoadResultText(loadResult);
            printf("[GameBoyScreen] Save load failed: %s\n",
                   this->error_message.c_str());
            return;
        }
        this->saveWarning = loadResult == SDLoadResult::SAVE_DAMAGED ||
            loadResult == SDLoadResult::SAVE_RECOVERED;
        this->saveWasReset = loadResult == SDLoadResult::SAVE_DAMAGED;
        printf("[GameBoyScreen] Save %s\n",
               saveFound ? "restored" :
               (this->saveWarning ? "damaged; starting clean" :
                                    "not present; starting clean"));
        this->cart_ram_dirty = false;
        this->cart_ram_last_write_us = 0;
        this->cart_ram_dirty_since_us = 0;
    }

    gb_init_lcd(static_cast<gb_s *>(this->gb_ptr), &lcd_draw_line);
    gb_load_palette(static_cast<gb_s *>(this->gb_ptr));
    this->lastFrameTimeUS = time_us_64();
    // Seed one frame so the first visible image does not wait a full period.
    this->frameAccumulatorUS = GAME_BOY_FRAME_US;
    printf("[GameBoyScreen] Loaded %s\n", this->romPath.c_str());
}

bool GameBoyScreen::saveCartridgeRAM() {
    if (!this->cart_ram || this->cart_ram_size == 0 ||
        !this->cart_ram_dirty || this->savePath.empty()) {
        return true;
    }

    SDLoadResult saveResult = SDLoadResult::WRITE_FAILED;
    if (!sdWriteSaveFile(this->savePath.c_str(), this->cart_ram,
                         this->cart_ram_size, &saveResult)) {
        this->saveFailed = true;
        printf("[GameBoyScreen] Could not save %s: %s\n",
               this->savePath.c_str(), sdLoadResultText(saveResult));
        return false;
    }
    this->cart_ram_dirty = false;
    this->cart_ram_last_write_us = 0;
    this->cart_ram_dirty_since_us = 0;
    this->saveFailed = false;
    this->saveWarning = false;
    this->saveWasReset = false;
    this->nextSaveAttemptUS = 0;
    return true;
}

void GameBoyScreen::pauseEmulator() {
    if (!this->gb_ptr || this->paused)
        return;
    gb_s *gb = static_cast<gb_s *>(this->gb_ptr);
    this->paused = true;
    this->exitAfterSave = false;
    this->saveRequested = this->cart_ram_dirty;
    this->nextSaveAttemptUS = 0;
    gb->direct.joypad = 0xFF;
    this->pendingJoypadPresses = 0;
    this->pendingJoypadReleases = 0;
    this->resumeAPressed = false;
    this->lastFrameTimeUS = time_us_64();
    this->frameAccumulatorUS = 0;
    printf("[GameBoyScreen] Paused\n");
}

void GameBoyScreen::resumeEmulator() {
    if (!this->gb_ptr || !this->paused)
        return;
    this->paused = false;
    this->exitAfterSave = false;
    this->lastFrameTimeUS = time_us_64();
    this->frameAccumulatorUS = GAME_BOY_FRAME_US;
    printf("[GameBoyScreen] Resumed\n");
}

GameBoyScreen::~GameBoyScreen() {
    this->saveCartridgeRAM();
    this->display = nullptr;
    if (this->gb_ptr) {
        delete static_cast<gb_s *>(this->gb_ptr);
        this->gb_ptr = nullptr;
    }
    free(this->cart_ram);
    this->cart_ram = nullptr;
    if (this->rom_owned)
        free(this->rom);
    this->rom = nullptr;
    delete[] this->palette;
    this->palette = nullptr;
}

void GameBoyScreen::update(uint16_t deltaTimeMS) {
    (void)deltaTimeMS;
    if (this->gb_ptr == nullptr || this->load_failed || this->emulator_error)
        return;

    gb_s *gb = static_cast<gb_s *>(this->gb_ptr);
    uint64_t now = time_us_64();
    const bool autoSaveDue = this->cart_ram_dirty &&
        this->cart_ram_last_write_us != 0 &&
        now - this->cart_ram_last_write_us >= AUTO_SAVE_IDLE_US;
    const bool maximumSaveDelayReached = this->cart_ram_dirty &&
        this->cart_ram_dirty_since_us != 0 &&
        now - this->cart_ram_dirty_since_us >= AUTO_SAVE_MAX_DELAY_US;
    if (this->cart_ram_dirty &&
        (this->saveRequested || autoSaveDue || maximumSaveDelayReached) &&
        now >= this->nextSaveAttemptUS) {
        const bool saved = this->saveCartridgeRAM();
        now = time_us_64();
        if (saved) {
            this->saveRequested = false;
        } else {
            this->saveRequested = true;
            this->nextSaveAttemptUS = now + SAVE_RETRY_US;
        }
        this->lastFrameTimeUS = now;
        this->frameAccumulatorUS = 0;
        if (saved && this->exitAfterSave && !this->exitRequested) {
            this->exitRequested = true;
            this->returnCallBack(this->screenId, this->option);
        }
    }
    if (this->paused) {
        // Do not accumulate a large emulation debt while paused.
        this->lastFrameTimeUS = now;
        this->frameAccumulatorUS = 0;
        return;
    }

    uint64_t elapsed = now - this->lastFrameTimeUS;
    this->lastFrameTimeUS = now;

    const uint64_t maxCatchUpUS = GAME_BOY_FRAME_US * MAX_CATCH_UP_FRAMES;
    elapsed = std::min(elapsed, maxCatchUpUS);
    this->frameAccumulatorUS = std::min(
        this->frameAccumulatorUS + elapsed, maxCatchUpUS);

    const uint8_t framesToRun = static_cast<uint8_t>(
        this->frameAccumulatorUS / GAME_BOY_FRAME_US);
    if (framesToRun == 0)
        return;
    this->frameAccumulatorUS -= framesToRun * GAME_BOY_FRAME_US;

    // Keep CPU, timers and interrupts at the real 59.7 Hz Game Boy cadence,
    // but compose only the final frame that can actually reach the ST7789.
    // Peanut-GB starts/ends gb_run_frame() at VBlank, so temporarily disabling
    // the callback cannot leave a partial scanline behind.
    const auto drawLine = gb->display.lcd_draw_line;
    for (uint8_t frame = 0; frame < framesToRun; frame++) {
        gb->display.lcd_draw_line =
            frame + 1 == framesToRun ? drawLine : nullptr;
        gb_run_frame(gb);
        if (frame == 0) {
            // A tap must survive one emulated frame, but not every frame in a
            // catch-up burst.
            this->pendingJoypadPresses = 0;
            gb->direct.joypad |= this->pendingJoypadReleases;
            this->pendingJoypadReleases = 0;
        }
    }
    gb->display.lcd_draw_line = drawLine;

    // if(!((gb_s*)this->gb_ptr)->direct.frame_skip) {
    //     audio_callback(NULL, (int16_t*)audioStream, AUDIO_BUFFER_SIZE_BYTES);
    //     int id = audio_play_once((uint8_t*)audioStream, AUDIO_BUFFER_SIZE_BYTES);
    //     if (id >= 0) 
    //         audio_source_set_volume(id, 1024);
    // }
}

void GameBoyScreen::draw(Display *display) {
    if(this->display == NULL) {
        display->clear(BLACKCOLOR);
        this->display = display;
    }

    if (this->load_failed || this->emulator_error) {
        display->clear(BLACKCOLOR);
        std::string title = "MICROSD ERROR";
        uint16_t width = alphanumfont.getTextWidth(title, 2);
        alphanumfont.drawText(display, title,
                             Vec2((DISPLAY_WIDTH - width) / 2, 78), 255, 2);

        width = alphanumfont.getTextWidth(this->error_message, 1);
        alphanumfont.drawText(display, this->error_message,
                             Vec2((DISPLAY_WIDTH - width) / 2, 122));

        width = alphanumfont.getTextWidth(this->romPath, 1);
        alphanumfont.drawText(display, this->romPath,
                             Vec2((DISPLAY_WIDTH - width) / 2, 148));

        std::string back = "SELECT TO RETURN";
        width = alphanumfont.getTextWidth(back, 1);
        alphanumfont.drawText(display, back,
                             Vec2((DISPLAY_WIDTH - width) / 2, 184));
    } else if (this->paused) {
        display->fillRect(Rect2(34, 68, 252, 108), BLACKCOLOR, 230);

        std::string title = this->saveFailed ? "SAVE FAILED" :
            (this->saveWarning
                ? (this->saveWasReset ? "SAVE RESET" : "SAVE RECOVERED")
                : "PAUSED");
        uint16_t width = alphanumfont.getTextWidth(title, 2);
        alphanumfont.drawText(display, title,
                             Vec2((DISPLAY_WIDTH - width) / 2, 82), 255, 2);

        std::string resume = this->saveFailed ? "CHECK SD - RETRYING" :
            (this->saveWarning
                ? (this->saveWasReset ? "DAMAGED SAVE PRESERVED" :
                                        "VALID SAVE RESTORED")
                :
                                 "A OR SELECT RESUME");
        width = alphanumfont.getTextWidth(resume);
        alphanumfont.drawText(display, resume,
                             Vec2((DISPLAY_WIDTH - width) / 2, 128));

        std::string exit = this->saveFailed ? "B RETRY SAVE" : "B EXIT";
        width = alphanumfont.getTextWidth(exit);
        alphanumfont.drawText(display, exit,
                             Vec2((DISPLAY_WIDTH - width) / 2, 150));
    } else if (this->saveFailed || this->saveWarning) {
        display->fillRect(Rect2(42, 218, 236, 18), BLACKCOLOR, 230);
        std::string warning = this->saveFailed
            ? "SAVE FAILED - RETRYING"
            : (this->saveWasReset ? "SAVE DAMAGED - NEW GAME"
                                  : "VALID SAVE RESTORED");
        const uint16_t width = alphanumfont.getTextWidth(warning);
        alphanumfont.drawText(display, warning,
                             Vec2((DISPLAY_WIDTH - width) / 2, 223));
    }
}

void GameBoyScreen::keyPressed(uint8_t key) {
    if (this->load_failed || this->emulator_error || !this->gb_ptr) {
        if (key == KEY_SELECT || key == KEY_B)
            this->returnCallBack(this->screenId, this->option);
        return;
    }

    gb_s *gb = (gb_s*)this->gb_ptr;

    // A short SELECT press belongs to the cartridge. Holding it opens the
    // system pause menu, so games such as Pokemon retain their full controls.
    if (key == KEY_SELECT) {
        if (this->paused) {
            this->selectHoldTriggered = true;
            this->resumeEmulator();
            return;
        } else {
            this->selectPressedAtUS = time_us_64();
            this->selectHoldTriggered = false;
            return;
        }
    }

    if (this->paused) {
        if (key == KEY_A) {
            this->resumeAPressed = true;
            this->resumeEmulator();
        } else if (key == KEY_B && !this->exitRequested) {
            if (this->cart_ram_dirty) {
                this->exitAfterSave = true;
                this->saveRequested = true;
                this->nextSaveAttemptUS = 0;
            } else {
                this->exitRequested = true;
                this->returnCallBack(this->screenId, this->option);
            }
        }
        return;
    }

    const uint8_t mask = joypadMaskForKey(key);
    if (mask != 0) {
        gb->direct.joypad &= static_cast<uint8_t>(~mask);
        this->pendingJoypadPresses |= mask;
        this->pendingJoypadReleases &= static_cast<uint8_t>(~mask);
    }
}

void GameBoyScreen::keyReleased(uint8_t key) {
    if (!this->gb_ptr || this->load_failed || this->emulator_error)
        return;

    gb_s *gb = (gb_s*)this->gb_ptr;

    if (key == KEY_A && this->resumeAPressed) {
        this->resumeAPressed = false;
        gb->direct.joypad |= joypadMaskForKey(KEY_A);
        return;
    }

    if (key == KEY_SELECT) {
        const uint64_t pressedAt = this->selectPressedAtUS;
        this->selectPressedAtUS = 0;
        if (this->selectHoldTriggered) {
            this->selectHoldTriggered = false;
            return;
        }
        if (pressedAt == 0 || this->paused)
            return;
        if (time_us_64() - pressedAt >= SYSTEM_PAUSE_HOLD_US) {
            this->pauseEmulator();
            return;
        }

        // SELECT is deferred until release, allowing a long press to remain a
        // pure system gesture. A short press is held for one emulated frame.
        const uint8_t mask = joypadMaskForKey(KEY_SELECT);
        gb->direct.joypad &= static_cast<uint8_t>(~mask);
        this->pendingJoypadPresses |= mask;
        this->pendingJoypadReleases |= mask;
        return;
    }

    const uint8_t mask = joypadMaskForKey(key);
    if (mask == 0)
        return;

    if ((this->pendingJoypadPresses & mask) != 0)
        this->pendingJoypadReleases |= mask;
    else
        gb->direct.joypad |= mask;
}

void GameBoyScreen::keyDown(uint8_t key) {
    if (!this->gb_ptr || this->load_failed || this->emulator_error ||
        this->paused) {
        return;
    }

    if (key == KEY_SELECT) {
        if (!this->selectHoldTriggered && this->selectPressedAtUS != 0 &&
            time_us_64() - this->selectPressedAtUS >=
                SYSTEM_PAUSE_HOLD_US) {
            this->selectHoldTriggered = true;
            this->pauseEmulator();
        }
        return;
    }

    if (key == KEY_A && this->resumeAPressed)
        return;

    // Reapply a non-system key that remained physically held across pause.
    const uint8_t mask = joypadMaskForKey(key);
    if (mask != 0)
        static_cast<gb_s *>(this->gb_ptr)->direct.joypad &=
            static_cast<uint8_t>(~mask);
}
