#include "../include/hello_syscall.h"

.syntax unified
.cpu cortex-a9
.arm

.global minemu_hello_uart_putchar
.type minemu_hello_uart_putchar, %function

minemu_hello_uart_putchar:
    push {r7, lr}
    mov r7, #MINEMU_HELLO_SYSCALL_UART_PUTCHAR
    svc #0
    pop {r7, pc}
