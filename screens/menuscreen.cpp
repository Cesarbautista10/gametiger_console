#include "menuscreen.h"
#include "../core/keyboard.h"

MenuScreen::MenuScreen(void (*rcb)(int8_t menu, uint8_t option), void (*hscb)(uint32_t highscore), uint32_t highscore, uint8_t option) {
    printf("[MenuScreen] loading...\n");
    this->screenId = ScreenEnum::MENUSCREEN;
    this->returnCallBack = rcb;
    this->highScoreCallBack = hscb;
    this->currentMenuItem = highscore;
    this->selectedMenuItem = highscore;
    this->isAnimating = false;
    this->animationCounter = 0;
    this->currentOptionItem = 1;
    this->option = option;
    this->gameBoyRomCount = 0;
    this->gameBoyScanResult = SDLoadResult::READ_FAILED;
    this->gameBoyBrowserOpen = false;

    uint16_t tileWidth = 40, tileHeight = 40;
    uint8_t xCount = (DISPLAY_WIDTH / tileWidth);
    uint8_t yCount = (DISPLAY_HEIGHT / tileHeight);
    uint16_t *ts = new uint16_t[xCount*yCount];
    bool colorFlip = true;
    for (int y = 0; y < yCount; y++) {
        for (int x = 0; x < xCount; x++) {
            colorFlip = !colorFlip;
            ts[y*xCount+x] = colorFlip ? 1 : 2;
        }
        colorFlip = !colorFlip;
    }
    this->bgLayer = new TileMap(xCount, yCount, tileWidth, tileHeight, ts);

    TileInfo tinfo; 
    tinfo.color = Color(0, 0, 0);
    tinfo.type = COLORFILL;
    this->bgLayer->addTileInfo(1, tinfo);
    tinfo.color = Color(0, 0, 0);
    this->bgLayer->addTileInfo(2, tinfo);
    if (this->currentMenuItem == 1 && this->option > 0)
        this->currentOptionItem = this->option;
    printf("[MenuScreen] Done\n");
}

MenuScreen::~MenuScreen() {
    delete this->bgLayer;
    this->bgLayer = nullptr;
    printf("[MenuScreen] Destructing\n");
}

void MenuScreen::update(uint16_t deltaTimeMS) {
    (void)deltaTimeMS;
    if(this->isAnimating) {
        this->animationCounter += 16;
        if(this->animationCounter > this->menuItemGap) {
            this->animationCounter = 0;
            this->isAnimating = false;
            this->currentMenuItem = this->selectedMenuItem;
            this->currentOptionItem = 1;
            this->gameBoyBrowserOpen = false;
        }
    }
}

void MenuScreen::refreshGameBoyCatalog() {
    this->gameBoyRomCount = sdScanGameBoyROMs(&this->gameBoyScanResult);
    if (this->currentOptionItem == 0 ||
        this->currentOptionItem > this->gameBoyRomCount) {
        this->currentOptionItem = 1;
    }
}

uint8_t MenuScreen::currentOptionCount() const {
    if (this->currentMenuItem == 1)
        return static_cast<uint8_t>(this->gameBoyRomCount);

    uint8_t count = 0;
    for (uint8_t item = 1; item <= 6; ++item) {
        if (this->menuItemNames[this->currentMenuItem][item].empty())
            break;
        count = item;
    }
    return count;
}

const char *MenuScreen::currentOptionName() const {
    if (this->currentMenuItem == 1) {
        if (!this->gameBoyBrowserOpen)
            return "A OPEN";
        if (this->gameBoyRomCount == 0) {
            return this->gameBoyScanResult == SDLoadResult::OK
                ? "NO GB ROMS"
                : sdLoadResultText(this->gameBoyScanResult);
        }
        const SDGameBoyROM *rom =
            sdGameBoyROMAt(this->currentOptionItem - 1);
        return rom ? rom->filename : "ROM NOT FOUND";
    }

    return this->menuItemNames[this->currentMenuItem]
                              [this->currentOptionItem].c_str();
}

void MenuScreen::drawGameBoyBrowser(Display *display) {
    std::string title = "GAME BOY ROMS";
    uint16_t width = alphanumfont.getTextWidth(title, 2);
    alphanumfont.drawText(display, title,
                         Vec2((DISPLAY_WIDTH - width) / 2, 10), 255, 2);

    if (this->gameBoyRomCount == 0) {
        std::string message = this->gameBoyScanResult == SDLoadResult::OK
            ? "NO GB ROMS"
            : sdLoadResultText(this->gameBoyScanResult);
        width = alphanumfont.getTextWidth(message);
        alphanumfont.drawText(display, message,
                             Vec2((DISPLAY_WIDTH - width) / 2, 96));

        std::string hint = "START REFRESH";
        width = alphanumfont.getTextWidth(hint);
        alphanumfont.drawText(display, hint,
                             Vec2((DISPLAY_WIDTH - width) / 2, 132));
    } else {
        constexpr size_t visibleRows = 5;
        const size_t selected = this->currentOptionItem - 1;
        size_t first = selected > 2 ? selected - 2 : 0;
        if (first + visibleRows > this->gameBoyRomCount) {
            first = this->gameBoyRomCount > visibleRows
                ? this->gameBoyRomCount - visibleRows
                : 0;
        }
        size_t last = first + visibleRows;
        if (last > this->gameBoyRomCount)
            last = this->gameBoyRomCount;

        for (size_t index = first; index < last; ++index) {
            const SDGameBoyROM *rom = sdGameBoyROMAt(index);
            if (!rom)
                continue;

            std::string name = rom->filename;
            const std::string::size_type extension = name.rfind('.');
            if (extension != std::string::npos)
                name.erase(extension);

            std::string row = std::to_string(index + 1) + " " + name;
            const int16_t y = 52 + static_cast<int16_t>(index - first) * 26;
            if (index == selected)
                display->rect(Rect2(28, y - 3, 264, 22), WHITECOLOR);

            width = alphanumfont.getTextWidth(row);
            alphanumfont.drawText(display, row,
                                 Vec2((DISPLAY_WIDTH - width) / 2, y));
        }

        std::string position = "ROM " +
            std::to_string(this->currentOptionItem) + " OF " +
            std::to_string(this->gameBoyRomCount);
        width = alphanumfont.getTextWidth(position);
        alphanumfont.drawText(display, position,
                             Vec2((DISPLAY_WIDTH - width) / 2, 184));
    }

    std::string actions = "A OPEN  B BACK";
    width = alphanumfont.getTextWidth(actions);
    alphanumfont.drawText(display, actions,
                         Vec2((DISPLAY_WIDTH - width) / 2, 204));

    std::string navigation = "UP DOWN  START REFRESH";
    width = alphanumfont.getTextWidth(navigation);
    alphanumfont.drawText(display, navigation,
                         Vec2((DISPLAY_WIDTH - width) / 2, 222));
}

void MenuScreen::draw(Display *display) {
    this->bgLayer->draw(display, 0, 0);
    if (!this->isAnimating && this->currentMenuItem == 1 &&
        this->gameBoyBrowserOpen) {
        this->drawGameBoyBrowser(display);
        return;
    }

    for (int i = 0; i < menuCount; i++) {
        int posx = (i-this->currentMenuItem) * this->menuItemGap;
        if(isAnimating)
            posx += (this->currentMenuItem - this->selectedMenuItem) * this->animationCounter;
        int16_t deltaGap = (abs(posx) * 32) / this->menuItemGap;
        uint8_t alpha = 255 - (deltaGap * 4) - 1;
        uint8_t size = 96 - deltaGap;
        Rect2 itemRect = Rect2(posx + (DISPLAY_WIDTH - size)/2, (DISPLAY_HEIGHT - size)/2, size, size);
        menuSprite.drawSprite(display, menuItemFrames[i], itemRect, alpha);
    }
    if(!isAnimating) {
        uint16_t width = alphanumfont.getTextWidth(this->menuItemNames[this->currentMenuItem][0], 2);
        alphanumfont.drawText(display, this->menuItemNames[this->currentMenuItem][0], Vec2((DISPLAY_WIDTH - width)/2, 170), 255, 2);
        const char *optionName = this->currentOptionName();
        if(optionName[0] != '\0') {
            std::string option = optionName;
            width = alphanumfont.getTextWidth(option);
            alphanumfont.drawText(display, option, Vec2((DISPLAY_WIDTH - width)/2, 200));
        }
        if (this->currentMenuItem == 1 && this->gameBoyRomCount > 1) {
            std::string navigation = "UP DOWN " +
                std::to_string(this->currentOptionItem) + "/" +
                std::to_string(this->gameBoyRomCount);
            width = alphanumfont.getTextWidth(navigation);
            alphanumfont.drawText(display, navigation,
                                 Vec2((DISPLAY_WIDTH - width) / 2, 220));
        }
    }
}

void MenuScreen::keyPressed(uint8_t key) {
    if(this->isAnimating)
        return;

    if (this->currentMenuItem == 1 && this->gameBoyBrowserOpen) {
        if (key == KEY_UP) {
            if (this->currentOptionItem > 1)
                this->currentOptionItem--;
        } else if (key == KEY_DOWN) {
            if (this->currentOptionItem < this->currentOptionCount())
                this->currentOptionItem++;
        } else if (key == KEY_START) {
            this->refreshGameBoyCatalog();
        } else if (key == KEY_B) {
            this->gameBoyBrowserOpen = false;
        } else if (key == KEY_A && this->gameBoyRomCount > 0) {
            this->returnCallBack(this->currentMenuItem + 2,
                                 this->currentOptionItem);
        }
        return;
    }

    if(key == KEY_RIGHT) {
        if(this->selectedMenuItem < this->menuCount-1) {
            this->selectedMenuItem++;
            this->isAnimating = true;
        }
    } else if (key == KEY_LEFT) {
        if(this->selectedMenuItem != 0) {
            this->selectedMenuItem--;
            this->isAnimating = true;
        }
    } else if (key == KEY_UP) {
        if(this->currentOptionItem > 1)
            this->currentOptionItem--;
    } else if (key == KEY_DOWN) {
        if(this->currentOptionItem < this->currentOptionCount())
            this->currentOptionItem++;
    } else if(key == KEY_A) {
        if (this->currentMenuItem == 1) {
            this->gameBoyBrowserOpen = true;
            this->refreshGameBoyCatalog();
            return;
        }
        this->returnCallBack(this->currentMenuItem+2, this->currentOptionItem);
    }
}

void MenuScreen::keyReleased(uint8_t key) {
    // const char c[6] = {'U', 'D', 'L', 'R', 'A', 'B'};
    // printf("Key Released: %c\n", c[key]);
}

void MenuScreen::keyDown(uint8_t key){
    // const char c[6] = {'U', 'D', 'L', 'R', 'A', 'B'};
    // printf("Key Down: %c\n", c[key]);
}
