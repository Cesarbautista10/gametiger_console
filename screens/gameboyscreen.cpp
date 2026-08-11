#include "gameboyscreen.h"
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "../core/Peanut-GB/peanut_gb.h"
#include "../core/sd_storage.h"

namespace {
constexpr uint64_t GAME_BOY_FRAME_US = 16743;
constexpr uint64_t MAX_CATCH_UP_FRAMES = 4;

uint8_t joypadMaskForKey(uint8_t key) {
    switch (key) {
        case KEY_A:     return 1U << 0;
        case KEY_B:     return 1U << 1;
        case KEY_START: return 1U << 3;
        case KEY_RIGHT: return 1U << 4;
        case KEY_LEFT:  return 1U << 5;
        case KEY_UP:    return 1U << 6;
        case KEY_DOWN:  return 1U << 7;
        default:        return 0;
    }
}

bool validateGameBoyROM(const uint8_t *rom, size_t size, const char **error) {
    if (!rom || size < 0x150) {
        *error = "INVALID GB ROM";
        return false;
    }

    // Peanut-GB indexes fixed tables with these header bytes. Validate them
    // before gb_init() so a malformed/truncated file cannot index out of bounds.
    const uint8_t romSizeCode = rom[0x148];
    const uint8_t ramSizeCode = rom[0x149];
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
    if (rom[0x143] == 0xC0) {
        *error = "GBC NOT SUPPORTED";
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
	p->cart_ram[addr] = val;
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

GameBoyScreen::GameBoyScreen(void (*rcb)(int8_t menu, uint8_t option), void (*hscb)(uint32_t highscore), uint32_t highscore, uint8_t option) {
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
    this->romPath = "/roms";

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

    this->romPath = selectedROM->path;
    this->rom = sdLoadFile(this->romPath.c_str(), &this->rom_size, &loadResult);
    if (!this->rom) {
        this->load_failed = true;
        this->error_message = sdLoadResultText(loadResult);
        printf("[GameBoyScreen] ROM load failed: %s\n", this->error_message.c_str());
        return;
    }
    this->rom_owned = true;

    const char *validationError = nullptr;
    if (!validateGameBoyROM(this->rom, this->rom_size, &validationError)) {
        this->load_failed = true;
        this->error_message = validationError;
        free(this->rom);
        this->rom = nullptr;
        this->rom_size = 0;
        this->rom_owned = false;
        return;
    }

    this->gb_ptr = new gb_s();

    enum gb_init_error_e ret = gb_init((gb_s*)this->gb_ptr, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, this);
	if(ret != GB_INIT_NO_ERROR) {
		printf("[GameBoyScreen] gb_init failed: %d\n", ret);
		delete (gb_s*)this->gb_ptr;
		this->gb_ptr = nullptr;
		this->load_failed = true;
		this->error_message = "INVALID GB ROM";
		free(this->rom);
		this->rom = nullptr;
		this->rom_size = 0;
		this->rom_owned = false;
		return;
	}

    this->cart_ram_size = gb_get_save_size((gb_s*)this->gb_ptr);
    printf("[GameBoyScreen] cart_ram_size: %u\n",
           static_cast<unsigned>(this->cart_ram_size));
    if(this->cart_ram_size > 0) {
        this->cart_ram = (uint8_t*)calloc(this->cart_ram_size, 1);
        if (!this->cart_ram) {
            this->load_failed = true;
            this->error_message = "NO SAVE MEMORY";
            delete (gb_s*)this->gb_ptr;
            this->gb_ptr = nullptr;
            free(this->rom);
            this->rom = nullptr;
            this->rom_size = 0;
            this->rom_owned = false;
            return;
        }
    }

    gb_init_lcd((gb_s*)this->gb_ptr, &lcd_draw_line);
    gb_load_palette((gb_s*)this->gb_ptr);
    this->lastFrameTimeUS = time_us_64();
    // Seed one frame so the first visible image does not wait a full period.
    this->frameAccumulatorUS = GAME_BOY_FRAME_US;
    printf("[GameBoyScreen] Loaded %s\n", this->romPath.c_str());
}

GameBoyScreen::~GameBoyScreen() {
    this->display = NULL;
    if(this->gb_ptr) {
		delete (gb_s*)this->gb_ptr;
        this->gb_ptr = NULL;
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
    if(this->gb_ptr == NULL || this->load_failed || this->emulator_error)
        return;

    gb_s *gb = (gb_s*)this->gb_ptr;
    const uint64_t now = time_us_64();
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
    }
    gb->display.lcd_draw_line = drawLine;

    // Input is polled while the LCD DMA is busy. A short tap can therefore be
    // pressed and released entirely between two emulated frames. Keep such a
    // tap active until at least one Game Boy frame has consumed it.
    this->pendingJoypadPresses = 0;
    gb->direct.joypad |= this->pendingJoypadReleases;
    this->pendingJoypadReleases = 0;

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

        std::string title = "PAUSED";
        uint16_t width = alphanumfont.getTextWidth(title, 2);
        alphanumfont.drawText(display, title,
                             Vec2((DISPLAY_WIDTH - width) / 2, 82), 255, 2);

        std::string resume = "A OR SELECT RESUME";
        width = alphanumfont.getTextWidth(resume);
        alphanumfont.drawText(display, resume,
                             Vec2((DISPLAY_WIDTH - width) / 2, 128));

        std::string exit = "B EXIT";
        width = alphanumfont.getTextWidth(exit);
        alphanumfont.drawText(display, exit,
                             Vec2((DISPLAY_WIDTH - width) / 2, 150));
    }
}

void GameBoyScreen::keyPressed(uint8_t key) {
    if (this->load_failed || this->emulator_error || !this->gb_ptr) {
        if (key == KEY_SELECT || key == KEY_B)
            this->returnCallBack(this->screenId, this->option);
        return;
    }

    gb_s *gb = (gb_s*)this->gb_ptr;

    // SELECT toggles GameConsole's system pause menu. START is never consumed
    // by the frontend, because many ROMs require it on their title screen.
    if (key == KEY_SELECT) {
        if (this->paused) {
            this->paused = false;
            this->lastFrameTimeUS = time_us_64();
            this->frameAccumulatorUS = GAME_BOY_FRAME_US;
            printf("[GameBoyScreen] Resumed\n");
        } else {
            this->paused = true;
            gb->direct.joypad = 0xFF;
            this->pendingJoypadPresses = 0;
            this->pendingJoypadReleases = 0;
            this->lastFrameTimeUS = time_us_64();
            this->frameAccumulatorUS = 0;
            printf("[GameBoyScreen] Paused\n");
        }
        return;
    }

    if (this->paused) {
        if (key == KEY_A) {
            this->paused = false;
            this->lastFrameTimeUS = time_us_64();
            this->frameAccumulatorUS = GAME_BOY_FRAME_US;
            printf("[GameBoyScreen] Resumed\n");
        } else if (key == KEY_B && !this->exitRequested) {
            this->exitRequested = true;
            this->returnCallBack(this->screenId, this->option);
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

    const uint8_t mask = joypadMaskForKey(key);
    if (mask == 0)
        return;

    if ((this->pendingJoypadPresses & mask) != 0)
        this->pendingJoypadReleases |= mask;
    else
        gb->direct.joypad |= mask;
}

void GameBoyScreen::keyDown(uint8_t key) {
}
