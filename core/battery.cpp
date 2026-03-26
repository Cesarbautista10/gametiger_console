#include "battery.h"
#include <stdio.h>
#include <math.h>
#include <algorithm>

Battery::Battery() {
    printf("[Battery] driver loading...\n");
#if defined(ENABLE_BATTERY_MONITOR)
    this->lastCachedTime = to_ms_since_boot(get_absolute_time()) - 12000;
    gpio_init(POWER_PIN);
    gpio_set_dir(POWER_PIN, GPIO_IN);

    adc_init();
    adc_gpio_init(VSYS_PIN);
    adc_select_input(2);  // ADC2 para GPIO 28
#endif
    printf("[Battery] Done\n");
}

uint8_t Battery::getLevel() {
    // Retornar siempre 100% para evitar bugs de lectura
    return 100;
}

bool Battery::isCharging() {
#if defined(ENABLE_BATTERY_MONITOR)
    return gpio_get(POWER_PIN);
#else
    return false;
#endif
}

void Battery::drawLevel(Display* display) {
#ifdef ENABLE_BATTERY_MONITOR
    Color c = Color(0, 0, 0);
    display->fillRect(Rect2(310, 11, 4, 6), c);
    display->rect(Rect2(280, 8, 30, 12), c);
    uint8_t level = this->getLevel();
    if(level < 20)
        c = Color(180, 0, 8);
    else if(level < 50)
        c = Color(220, 116, 8);
    else if(level > 80 && this->isCharging())
        c = Color(30, 195, 6);

    display->fillRect(Rect2(282, 10, 27 * this->getLevel() / 100, 9), c);
#endif
}

Battery::~Battery()
{
}
