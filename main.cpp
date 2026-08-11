#include "core/common.h"
#include "core/display.h"
#include "core/battery.h"
#include "core/keyboard.h"
#include "core/flash_storage.h"
#include "core/LoRa/lora.h"

#include "screens/splashscreen.h"
#include "screens/menuscreen.h"
#include "screens/snakescreen.h"
#include "screens/gameboyscreen.h"
#include "screens/g2048screen.h"
#include "screens/tetrisscreen.h"
#include "screens/minescreen.h"
#include "screens/ticscreen.h"
#include "screens/pa2screen.h"
#include "screens/aboutscreen.h"
#include "screens/settingsscreen.h"

#define HIGHSCORESIZE (FLASH_PAGE_SIZE/4)
#define FLASH_TARGET_OFFSET (1536 * 1024)

Screen *screen;
Lora *lora;
uint32_t highscores[64];
bool shouldSwitchScreen;
uint8_t newScreenId, newOption;

void highScoreHandler(uint32_t highscore) {
    highscores[0] = 64;highscores[1] = 128;
    highscores[screen->screenId] = highscore;
    if (!flashStorageErase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE) ||
        !flashStorageProgram(FLASH_TARGET_OFFSET,
                             reinterpret_cast<const uint8_t *>(highscores),
                             FLASH_PAGE_SIZE)) {
        printf("[Main] High-score flash write failed\n");
    }
}

void readHighScoreData() {
    const uint32_t* flash_target_contents = (const uint32_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
    for (int i = 0; i < HIGHSCORESIZE; i++)
        highscores[i] = flash_target_contents[i];
    if(highscores[0] != 64 || highscores[1] != 128)
        for (int i = 0; i < HIGHSCORESIZE; i++)
            highscores[i] = 0;
}

void backHandler(int8_t menu, uint8_t option) {
    newScreenId = menu;
    newOption = option;
    shouldSwitchScreen = true;
}

void drawLoadingWindow(Display *display) {
    constexpr int16_t windowX = 48;
    constexpr int16_t windowY = 70;
    constexpr int16_t windowWidth = 224;
    constexpr int16_t windowHeight = 100;

    display->fillRect(Rect2(windowX, windowY, windowWidth, windowHeight),
                      BLACKCOLOR);
    display->rect(Rect2(windowX, windowY, windowWidth, windowHeight),
                  WHITECOLOR);
    display->rect(Rect2(windowX + 3, windowY + 3,
                        windowWidth - 6, windowHeight - 6), WHITECOLOR);

    const std::string title = "CARGANDO";
    uint16_t width = alphanumfont.getTextWidth(title, 2);
    alphanumfont.drawText(display, title,
                         Vec2((DISPLAY_WIDTH - width) / 2, windowY + 23),
                         255, 2);

    const std::string message = "ESPERE...";
    width = alphanumfont.getTextWidth(message);
    alphanumfont.drawText(display, message,
                         Vec2((DISPLAY_WIDTH - width) / 2, windowY + 66));

    // Complete the transfer before starting SD/flash work. The loading
    // window then remains on the LCD throughout the blocking ROM load.
    display->update();
}

void checkScreenSwitch(Display *display) {
    if(!shouldSwitchScreen)
        return;

    if(screen->screenId == ScreenEnum::MENUSCREEN) {
        if(newScreenId == ScreenEnum::GAMEBOYSCREEN)
            drawLoadingWindow(display);
        delete screen;
        if(newScreenId == ScreenEnum::SNAKESCREEN)
            screen = new SnakeScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::GAMEBOYSCREEN)
            screen = new GameBoyScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::G2048SCREEN)
            screen = new G2048Screen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::TETRISSCREEN)
            screen = new TetrisScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::MINESCREEN)
            screen = new MineScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::TICSCREEN)
            screen = new TicScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::PA2SCREEN)
            screen = new PixelAdventureScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::SETTINGSSCREEN)
            screen = new SettingsScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else if(newScreenId == ScreenEnum::ABOUTSCREEN)
            screen = new AboutScreen(*backHandler, *highScoreHandler, highscores[newScreenId], newOption);
        else
            printf("[Main] Something failed badly\n");
    } else if(screen->screenId == ScreenEnum::SPLASHSCREEN) {
        delete screen;
        screen = new MenuScreen(*backHandler, *highScoreHandler, newScreenId, newOption);
    } else {
        delete screen;
        screen = new MenuScreen(*backHandler, *highScoreHandler, newScreenId-2, newOption);
    }
    shouldSwitchScreen = false;
}

int main(int argc, char *argv[]) {
    sleep_ms(50);

    stdio_init_all();
    // USB stdio continues enumerating asynchronously; do not leave the LCD
    // black for a full second just to wait for an optional serial monitor.
    sleep_ms(100);

    printf("[Main] Starting\n");
    srand((unsigned int)time(0));
    readHighScoreData();
    Display *display = new Display();
    display->initDMAChannel();
    display->initSequence();
    display->clear(Color(0, 0, 0));
    display->update();

    Battery *battery = new Battery();
    KeyBoard *keyboard = new KeyBoard();
    screen = new SplashScreen(*backHandler, *highScoreHandler, 0, 1);

    timetype lastUpdate = getTime();
    bool close = false;
    while (!close) {
        uint16_t deltaTimeMS = getTimeDiffMS(lastUpdate);
        lastUpdate = getTime();

        keyboard->checkKeyState(screen);
        screen->update(deltaTimeMS);
        screen->draw(display);

        battery->drawLevel(display);
        // printf("[Main] FPS: %d\n", int(1000 / deltaTimeMS));
        display->beginUpdate();
        while(display->updateInProgress()) {
            // The display DMA occupies most of every frame. Use that time to
            // keep both I2C controllers near their native 100 Hz cadence,
            // while suppressing held-key repeats until the next game frame.
            keyboard->checkKeyState(screen, false);
            tight_loop_contents();
        }
        display->finishUpdate();
        checkScreenSwitch(display);
    }

    return EXIT_SUCCESS;
}
