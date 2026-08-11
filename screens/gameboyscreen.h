#include "../core/common.h"
#include "../core/screen.h"
#include "../core/keyboard.h"

class GameBoyScreen : public Screen
{
private:
    uint8_t option;
    timetype lastUpdate;
	uint64_t lastFrameTimeUS;
	uint64_t frameAccumulatorUS;
    bool exitRequested;
    bool paused;
    uint8_t pendingJoypadPresses;
    uint8_t pendingJoypadReleases;
    uint64_t selectPressedAtUS;
    bool selectHoldTriggered;
    bool resumeAPressed;
    bool saveRequested;
    bool saveFailed;
    bool saveWarning;
    bool saveWasReset;
    bool exitAfterSave;
    uint64_t nextSaveAttemptUS;
    std::string romPath;
    std::string savePath;

    void* gb_ptr;
    void pauseEmulator();
    void resumeEmulator();
    bool saveCartridgeRAM();

public:
    GameBoyScreen(void (*returnCallBack)(int8_t menu, uint8_t option), void (*highScoreCallBack)(uint32_t highscore), uint32_t highscore, uint8_t option);
    ~GameBoyScreen();

    /* Pointer to allocated memory holding GB file. */
    uint8_t *rom;
    size_t rom_size;
    bool rom_owned;
    /* Pointer to allocated memory holding save file. */
    uint8_t *cart_ram;
    size_t cart_ram_size;
    bool cart_ram_dirty;
    uint64_t cart_ram_last_write_us;
    uint64_t cart_ram_dirty_since_us;

    bool load_failed;
    bool emulator_error;
    std::string error_message;

    Display *display;
    
    Color *palette;

    void update(uint16_t deltaTimeMS);
    void draw(Display *display);
    void keyPressed(uint8_t key);
    void keyReleased(uint8_t key);
    void keyDown(uint8_t key);
};
