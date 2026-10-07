#include "minemu/boot.h"
#include "minemu/platform.h"
#include "minemu/trap.h"

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    const char message[] = "minemu raw MMIO example\n";
    for (size_t index = 0; index < sizeof(message) - 1; ++index) {
        MINEMU_UART0->tx_data = (uint8_t)message[index];
    }
    MINEMU_RNG->seed = UINT32_C(0x12345678);
    MINEMU_TRACE->event = MINEMU_RNG->data;
    MINEMU_SYSTICK->period = 100;
    MINEMU_SYSTICK->control = MINEMU_SYSTICK_CONTROL_ENABLE;
    MINEMU_BLOCK->control = MINEMU_BLOCK_CONTROL_IRQ_ENABLE;
    (void)boot_info;
    minemu_fail_stop();
}
