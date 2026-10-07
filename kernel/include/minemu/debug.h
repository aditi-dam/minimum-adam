#ifndef MINEMU_DEBUG_H
#define MINEMU_DEBUG_H

/* The tag must be a compile-time constant in the range 0 through 65535.
 * The memory clobber keeps surrounding memory accesses at the checkpoint. */
#define breakpoint(tag) \
    do { \
        __asm__ volatile("bkpt #%c0" : : "i"(tag) : "memory"); \
    } while (0)

#endif
