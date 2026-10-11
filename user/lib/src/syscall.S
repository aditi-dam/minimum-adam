#include "../../../kernel/include/minemu/user_abi.h"

.syntax unified
.cpu cortex-a9
.arm

.global ioctl
.type ioctl, %function

ioctl:
    push {r7, lr}
    mov r7, #MINEMU_SYSCALL_IOCTL
    svc #0
    pop {r7, pc}

.size ioctl, . - ioctl
