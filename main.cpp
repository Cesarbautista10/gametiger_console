#include "core/common.h"
#include "core/display.h"
#include "core/battery.h"
#include "core/keyboard.h"
#include "core/audio.h"
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
Audio *audio;
uint32_t highscores[64];
bool shouldSwitchScreen;
uint8_t newScreenId, newOption;

void highScoreHandler(uint32_t highscore) {
    highscores[0] = 64;highscores[1] = 128;
    highscores[screen->screenId] = highscore;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, (uint8_t*)highscores, FLASH_PAGE_SIZE);
    restore_interrupts (ints);
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

void checkScreenSwitch() {
    if(!shouldSwitchScreen)
        return;
    
    printf("[Main] Switching screen - current: %d, new: %d\n", screen->screenId, newScreenId);
    
    // Reproducir sonido de cambio de menú
    if(audio) audio->playMenuSound();

    
    if(screen->screenId == ScreenEnum::MENUSCREEN) {
        delete screen;
        printf("[Main] From MENU to screen %d\n", newScreenId);
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
            printf("[Main] Something failed badly - unknown screenId: %d\n", newScreenId);
    } else if(screen->screenId == ScreenEnum::SPLASHSCREEN) {
        delete screen;
        printf("[Main] From SPLASH to MENU\n");
        screen = new MenuScreen(*backHandler, *highScoreHandler, newScreenId, newOption);
    } else {
        delete screen;
        // Convertir screenId de vuelta al índice del menú
        uint8_t menuIndex = 0;
        if(newScreenId == ScreenEnum::SNAKESCREEN) menuIndex = 0;
        else if(newScreenId == ScreenEnum::GAMEBOYSCREEN) menuIndex = 1;
        else if(newScreenId == ScreenEnum::G2048SCREEN) menuIndex = 2;
        else if(newScreenId == ScreenEnum::TETRISSCREEN) menuIndex = 3;
        else if(newScreenId == ScreenEnum::MINESCREEN) menuIndex = 4;
        else if(newScreenId == ScreenEnum::TICSCREEN) menuIndex = 5;
        else if(newScreenId == ScreenEnum::PA2SCREEN) menuIndex = 6;
        else if(newScreenId == ScreenEnum::SETTINGSSCREEN) menuIndex = 7;
        else if(newScreenId == ScreenEnum::ABOUTSCREEN) menuIndex = 8;
        
        printf("[Main] From game %d back to MENU at index %d\n", newScreenId, menuIndex);
        screen = new MenuScreen(*backHandler, *highScoreHandler, menuIndex, newOption);
    }
    shouldSwitchScreen = false;
}

int main(int argc, char *argv[]) {
    sleep_ms(50);
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(1);
    set_sys_clock_khz(266 * 1000, true);
    sleep_ms(50);

    stdio_init_all();
    sleep_ms(1000);

    printf("[Main] Starting\n");
    srand((unsigned int)time(0));
    readHighScoreData();
    Display *display = new Display();
    display->initDMAChannel();
    display->initSequence();
    display->clear(Color(255, 255, 255));
    display->update();

    Battery *battery = new Battery();
    KeyBoard *keyboard = new KeyBoard();
    audio = new Audio();
    globalAudio = audio; // Hacer accesible globalmente
    
    // Melodía de arranque - Coin/Power-Up style (blocking OK aquí, antes del main loop)
    printf("[Main] Playing startup sound...\n");
    audio->playToneBlocking(988, 80);    // Si5
    sleep_ms(20);
    audio->playToneBlocking(1319, 80);   // Mi6
    sleep_ms(20);
    audio->playToneBlocking(1568, 80);   // Sol6
    sleep_ms(20);
    audio->playToneBlocking(2093, 200);  // Do7 (nota alta brillante)
    sleep_ms(60);
    audio->playToneBlocking(1568, 80);   // Sol6
    sleep_ms(20);
    audio->playToneBlocking(2093, 300);  // Do7 (cierre largo)
    
    screen = new SplashScreen(*backHandler, *highScoreHandler, 0, 1);

    timetype lastUpdate = getTime();
    bool close = false;
    while (!close) {
        uint16_t deltaTimeMS = getTimeDiffMS(lastUpdate);
        lastUpdate = getTime();

        screen->update(deltaTimeMS);
        keyboard->checkKeyState(screen);
        if (audio) audio->update();  // Non-blocking tone management
        screen->draw(display);

        battery->drawLevel(display);
        // printf("[Main] FPS: %d\n", int(1000 / deltaTimeMS));
        display->update();
        checkScreenSwitch();
    }

    return EXIT_SUCCESS;
}