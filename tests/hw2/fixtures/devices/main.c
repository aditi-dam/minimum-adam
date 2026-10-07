#include <stdint.h>

#include "minemu/user_abi.h"

static void trace(uint32_t value)
{
  (void)ioctl(MINEMU_FD_TRACE, MINEMU_IOCTL_TRACE_EVENT, value);
}

static void idle(void)
{
  __asm__ volatile("bkpt #0");
  while (1);
}

void minemu_user_main(void)
{
  uint32_t value;

  if (ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_STATE, &value) != 0) {
    idle();
  }
  trace(value);

  if (ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT, &value) != 0) {
    idle();
  }
  trace(value);

  if (ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_SEED, UINT32_C(0x12345678)) != 0 ||
      ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_STATE, &value) != 0) {
    idle();
  }
  trace(value);

  if (ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT, &value) != 0) {
    idle();
  }
  trace(value);
  idle();
}
