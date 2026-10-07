#include <stdint.h>

#include "minemu/boot.h"
#include "minemu/syscall.h"

enum { frame_words = sizeof(struct minemu_trap_frame) / sizeof(uint32_t), task_stack_words = 256 };

static uint32_t task_b_stack[task_stack_words] __attribute__((aligned(8)));
static uint32_t *task_a_frame;
static uint32_t *task_b_frame;

static void task_b(void) {
    for (;;) {
        __asm__ volatile("svc #0" : : : "memory");
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *current_frame) {
    if (task_b_frame == 0) {
        task_a_frame = (uint32_t *)current_frame;
        task_b_frame = task_b_stack + task_stack_words - frame_words;
        for (uint32_t index = 0; index < frame_words; ++index) {
            task_b_frame[index] = 0;
        }
        task_b_frame[13] = (uint32_t)(uintptr_t)task_b;
        task_b_frame[14] = current_frame->spsr;
        task_b_frame[15] = MINEMU_EXCEPTION_SVC;
        return (struct minemu_trap_frame *)task_b_frame;
    }
    task_b_frame = (uint32_t *)current_frame;
    return (struct minemu_trap_frame *)task_a_frame;
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    (void)boot_info;
    for (;;) {
        __asm__ volatile("svc #0" : : : "memory");
    }
}
