#include <stdint.h>
#include "minemu/user_abi.h"

// int32_t ioctl(int fd, uint32_t request, void* arg1, void* arg2);

void minemu_user_main(void) {
    static char message[] = "hello world\n";
    (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
        (void *)message, sizeof(message) - 1U);

    for (;;) {
        __asm__ volatile("nop");
    }

    // static const char message[] = "hello world\n";

    // // uint32_t uart0 = 0x10004000;
    // // uint32_t uart_write = 0x1;

    // uint32_t uart0 = MINEMU_FD_UART_OUT;
    // uint32_t uart_write = MINEMU_IOCTL_UART_READ;
    
    // ioctl(
    //     (int)uart0, uart_write, 
    //     (void*)message, sizeof(message) - 1
    // );

    // for (;;) {
    //     __asm__ volatile("nop");
    // }
}
