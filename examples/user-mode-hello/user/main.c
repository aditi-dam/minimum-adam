#include <stdint.h>

int32_t minemu_hello_uart_putchar(uint32_t byte);

void minemu_user_main(void) {
    static const char message[] = "hello world\n";
    for (uint32_t index = 0; index < sizeof(message) - 1U; ++index) {
        (void)minemu_hello_uart_putchar((uint8_t)message[index]);
    }
    for (;;) {
        __asm__ volatile("nop");
    }
}
