/*
 * GameTiger microSD hardware configuration.
 *
 * The card is used in SPI mode even though the socket signals are named
 * after their native SDIO functions:
 *   GPIO2  CLK, GPIO3 CMD/MOSI, GPIO4 DAT0/MISO, GPIO7 DAT3/CS.
 */

#include "hw_config.h"

static spi_t sd_spi = {
    .hw_inst = spi0,
    .miso_gpio = 4,
    .mosi_gpio = 3,
    .sck_gpio = 2,
    .baud_rate = 4000000,
    .spi_mode = 0,
};

static sd_spi_if_t sd_spi_interface = {
    .spi = &sd_spi,
    .ss_gpio = 7,
};

static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &sd_spi_interface,
};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    return num == 0 ? &sd_card : NULL;
}
