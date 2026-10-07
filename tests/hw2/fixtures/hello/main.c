#include <stdint.h>

#include "minemu/user_abi.h"

void minemu_user_main(void)
{
  static char message[] = "hello world\n";
  (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
    (void *)message, sizeof(message) - 1U);

  __asm__ volatile("bkpt #0");
  while (1);
}
