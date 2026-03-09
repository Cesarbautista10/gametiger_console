#ifndef _GAME_TIGER_LORA_H
#define _GAME_TIGER_LORA_H

#include "sx126x.h"
#include "sx126x_hal.h"

#if defined(PICO_RP2040) || defined(PICO_RP2350)
#include "../common.h"
#endif

class Lora
{
private:
	sx126x_hal_t context;
    void receiveData();
public:

    Lora();
    ~Lora();

	void SendData(int8_t power, char* data, uint8_t length);
    void ProcessIrq();
    void SetToReceiveMode();

    void SetTxEnable();
    void SetRxEnable();

    void CheckDeviceErrors();
    void CheckDeviceStatus();
};

#endif