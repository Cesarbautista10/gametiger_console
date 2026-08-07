#include "display.h"
#include <stdio.h>
#include <algorithm>
#include <memory>

Display::Display() {
    printf("[Display] driver loading...\n");
    this->frameBuffer = new FrameBuffer(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    this->initHardware();
    printf("[Display] Done\n");
}

void Display::initHardware() {
    printf("[Display] ST7789 Arduino-compatible v5 BGR+frame-sync, SPI target 20 MHz\n");

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, 1);

    gpio_init(RST_PIN);
    gpio_set_dir(RST_PIN, GPIO_OUT);
    gpio_put(RST_PIN, 1);

    gpio_init(DC_PIN);
    gpio_set_dir(DC_PIN, GPIO_OUT);

    gpio_init(BL_PIN);
    gpio_set_dir(BL_PIN, GPIO_OUT);
    this->setBrightness(100);

    spi_init(spi0, 20 * 1000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    uint br = spi_get_baudrate(spi0);
    printf("[Display] baudrate: %d\n", br);

    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
}

void Display::initDMAChannel() { 
    this->dmaSPIChannel = dma_claim_unused_channel(true);
    this->dmaSPIConfig = dma_channel_get_default_config(this->dmaSPIChannel);
    channel_config_set_transfer_data_size(&this->dmaSPIConfig, DMA_SIZE_16);
    channel_config_set_read_increment(&this->dmaSPIConfig, true);
    channel_config_set_write_increment(&this->dmaSPIConfig, false);
    channel_config_set_ring(&this->dmaSPIConfig, false, 0);
    channel_config_set_dreq(&this->dmaSPIConfig, DREQ_SPI0_TX);
}

void Display::initSequence() {
    this->reset();

    this->sendData(ST7789_SWRESET);
    sleep_ms(150);

    this->sendData(ST7789_SLPOUT);
    sleep_ms(10);

    this->sendData(ST7789_COLMOD, 0x55);
    sleep_ms(10);

    // Rotation 1 (320x240) plus BGR for GameTiger's RGB565 bit layout.
    this->sendData(ST7789_MADCTL, (uint8_t)0xA8);

    this->sendData(ST7789_INVON);  // Configuración de color correcta del panel
    this->sendData(ST7789_NORON);
    sleep_ms(10);
    this->sendData(ST7789_DISPON);
    sleep_ms(20);

    this->setWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void Display::reset() {
    gpio_put(CS_PIN, 1);
    
    gpio_put(RST_PIN, 1);
    sleep_ms(100);
    gpio_put(RST_PIN, 0);
    sleep_ms(100);
    gpio_put(RST_PIN, 1);
    sleep_ms(200);
}

void Display::setWindow(const uint16_t sx, const uint16_t sy, const uint16_t ex, const uint16_t ey){
    uint16_t x_start = sx + X_OFFSET;
    uint16_t y_start = sy + Y_OFFSET;
    uint16_t x_end = ex + X_OFFSET - 1;
    uint16_t y_end = ey + Y_OFFSET - 1;
    
    uint8_t buf1[] = {
        static_cast<uint8_t>(x_start >> 8), static_cast<uint8_t>(x_start),
        static_cast<uint8_t>(x_end >> 8), static_cast<uint8_t>(x_end)
    };
    this->sendData(ST7789_CASET, buf1, sizeof(buf1));

    uint8_t buf2[] = {
        static_cast<uint8_t>(y_start >> 8), static_cast<uint8_t>(y_start),
        static_cast<uint8_t>(y_end >> 8), static_cast<uint8_t>(y_end)
    };
    this->sendData(ST7789_RASET, buf2, sizeof(buf2));

    this->sendData(ST7789_RAMWR);
}

void Display::setCursor(const uint16_t x, const uint16_t y) {
    this->setWindow(x, y, x+1, y+1);
}

void Display::sendData(const uint8_t cmd, const uint8_t data[], size_t length) {
    gpio_put(CS_PIN, 0);
    this->write_cmd(cmd);
    this->write_data(data, length);
    gpio_put(CS_PIN, 1);
}

void Display::sendData(const uint8_t cmd, const uint8_t data) {
    gpio_put(CS_PIN, 0);
    this->write_cmd(cmd);
    this->write_data(data);
    gpio_put(CS_PIN, 1);
}

void Display::sendData(const uint8_t cmd) {
    gpio_put(CS_PIN, 0);
    this->write_cmd(cmd);
    gpio_put(CS_PIN, 1);
}

void Display::write_cmd(const uint8_t cmd) {
    gpio_put(DC_PIN, 0);
    uint8_t buf[] = {cmd};
    spi_write_blocking(spi0, buf, 1);
}

void Display::write_data(const uint8_t data) {
    gpio_put(DC_PIN, 1);
    uint8_t buf[] = {data};
    spi_write_blocking(spi0, buf, 1);
}

void Display::write_data(const uint8_t data[], size_t length) {
    gpio_put(DC_PIN, 1);
    spi_write_blocking(spi0, data, length);
}

void Display::setBrightness(uint8_t brightness) {
    gpio_put(BL_PIN, brightness > 0);
}

void Display::update() {
    // Reset the ST7789 address cursor for every frame.  Leaving RAMWR active
    // across frames caused the 320x240 image to wrap horizontally.
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    this->setWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(DC_PIN, 1);
    gpio_put(CS_PIN, 0);

    timetype lastUpdate = getTime();    
    dma_channel_configure(this->dmaSPIChannel, &this->dmaSPIConfig, &spi_get_hw(spi0)->dr, (uint16_t*)this->frameBuffer->buffer, DISPLAY_WIDTH * DISPLAY_HEIGHT, true);
    dma_channel_wait_for_finish_blocking(this->dmaSPIChannel);
    gpio_put(CS_PIN, 1);

    uint16_t deltaTimeMS = getTimeDiffMS(lastUpdate);
    // printf("[Display] Display Update: %d\n", deltaTimeMS);
}

Display::~Display() {
}

void Display::clear(Color c) {
    this->frameBuffer->clear(c);
}

void Display::setPixel(Vec2 pos, Color &c, uint8_t alpha) {
    this->frameBuffer->setPixel(pos, c, alpha);
}

void Display::drawBitmapRow(Vec2 pos, int width, Color *c) {
    this->frameBuffer->drawBitmapRow(pos, width, c);
}

void Display::fillRect(Rect2 rect, Color &c, uint8_t alpha) {
    this->frameBuffer->fillRect(rect, c, alpha);
}

void Display::hLine(Vec2 pos, int width, Color &c, uint8_t alpha) {
    this->frameBuffer->hLine(pos, width, c, alpha);
}

void Display::vLine(Vec2 pos, int height, Color &c, uint8_t alpha) {
    this->frameBuffer->vLine(pos, height, c, alpha);
}

void Display::rect(Rect2 rect, Color &c, uint8_t alpha) {
    this->frameBuffer->rect(rect, c, alpha);
}
