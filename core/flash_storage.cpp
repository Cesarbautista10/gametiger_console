#include "flash_storage.h"

#include "hardware/flash.h"
#include "pico/error.h"
#include "pico/flash.h"
#include "pico/platform.h"

namespace {
enum class FlashOperationType : uint8_t {
    ERASE,
    PROGRAM,
};

struct FlashOperation {
    FlashOperationType type;
    uint32_t offset;
    const uint8_t *data;
    size_t size;
};

void performFlashOperation(void *parameter) {
    const FlashOperation *operation =
        static_cast<const FlashOperation *>(parameter);
    if (operation->type == FlashOperationType::ERASE)
        flash_range_erase(operation->offset, operation->size);
    else
        flash_range_program(operation->offset, operation->data, operation->size);
}

bool rangeFitsFlash(uint32_t flashOffset, size_t size) {
    return flashOffset <= PICO_FLASH_SIZE_BYTES &&
        size <= PICO_FLASH_SIZE_BYTES - flashOffset;
}
}

bool flashStorageErase(uint32_t flashOffset, size_t size) {
    if (size == 0 || (flashOffset % FLASH_SECTOR_SIZE) != 0 ||
        (size % FLASH_SECTOR_SIZE) != 0 ||
        !rangeFitsFlash(flashOffset, size)) {
        return false;
    }

    const FlashOperation operation = {
        FlashOperationType::ERASE, flashOffset, nullptr, size
    };
    return flash_safe_execute(performFlashOperation,
                              const_cast<FlashOperation *>(&operation),
                              UINT32_MAX) == PICO_OK;
}

bool flashStorageProgram(uint32_t flashOffset, const uint8_t *data,
                         size_t size) {
    if (!data || size == 0 || (flashOffset % FLASH_PAGE_SIZE) != 0 ||
        (size % FLASH_PAGE_SIZE) != 0 ||
        !rangeFitsFlash(flashOffset, size)) {
        return false;
    }

    const FlashOperation operation = {
        FlashOperationType::PROGRAM, flashOffset, data, size
    };
    return flash_safe_execute(performFlashOperation,
                              const_cast<FlashOperation *>(&operation),
                              UINT32_MAX) == PICO_OK;
}
