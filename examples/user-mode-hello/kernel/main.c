#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../include/hello_syscall.h"
#include "minemu/boot.h"
#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"

#define BOOTSTRAP_PAGE_DIRECTORY_PADDR UINT32_C(0x40010000)
#define USER_IMAGE_BASE UINT32_C(0x00400000)
#define USER_STACK_BOTTOM UINT32_C(0x007ff000)
#define USER_STACK_TOP UINT32_C(0x00800000)
#define MAX_USER_IMAGE_PAGES 16U
#define SYSTEM_ROM_TABLE_COUNT 4U
#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)
#define TRACE_USER_SYSCALL UINT32_C(0x48454c10)

static uint32_t system_rom_tables[SYSTEM_ROM_TABLE_COUNT][1024]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint32_t user_page_table[1024] __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint8_t user_image_pages[MAX_USER_IMAGE_PAGES][MINEMU_PAGE_SIZE]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint8_t user_stack[MINEMU_PAGE_SIZE] __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint32_t user_page_vaddrs[MAX_USER_IMAGE_PAGES];
static uint32_t user_page_flags[MAX_USER_IMAGE_PAGES];
static size_t user_page_count;
static bool user_syscall_observed;

void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));

static void zero_bytes(void *destination, size_t length) {
    uint8_t *bytes = destination;
    for (size_t index = 0; index < length; ++index) {
        bytes[index] = 0;
    }
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *out = destination;
    const uint8_t *in = source;
    for (size_t index = 0; index < length; ++index) {
        out[index] = in[index];
    }
}

static bool bytes_equal(const uint8_t *left, const char *right, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (left[index] != (uint8_t)right[index]) {
            return false;
        }
    }
    return true;
}

static bool image_range_valid(uint32_t offset, uint32_t length, uint32_t image_size) {
    return offset <= image_size && length <= image_size - offset;
}

static bool image_records_valid(uint32_t offset, uint32_t count, uint32_t image_size) {
    return offset <= image_size && count <= ((image_size - offset) >> 5);
}

static uint32_t kernel_vaddr_to_paddr(const void *address) {
    return (uint32_t)(uintptr_t)address - MINEMU_KERNEL_DIRECT_BASE + MINEMU_RAM_BASE;
}

static volatile uint32_t *active_page_directory(void) {
    return (volatile uint32_t *)(uintptr_t)(
        MINEMU_KERNEL_DIRECT_BASE + BOOTSTRAP_PAGE_DIRECTORY_PADDR - MINEMU_RAM_BASE);
}

static bool map_system_rom(const struct minemu_boot_info *boot_info) {
    if (boot_info->system_rom_base != MINEMU_SYSTEM_ROM_BASE || boot_info->image_size == 0 ||
        boot_info->image_size > MINEMU_SYSTEM_ROM_SIZE) {
        return false;
    }

    uint32_t page_count = (boot_info->image_size + MINEMU_PAGE_SIZE - 1U) / MINEMU_PAGE_SIZE;
    uint32_t table_count = (page_count + 1023U) / 1024U;
    if (table_count > SYSTEM_ROM_TABLE_COUNT) {
        return false;
    }

    volatile uint32_t *directory = active_page_directory();
    for (uint32_t table = 0; table < table_count; ++table) {
        zero_bytes(system_rom_tables[table], sizeof(system_rom_tables[table]));
        uint32_t remaining = page_count - table * 1024U;
        uint32_t entries = remaining < 1024U ? remaining : 1024U;
        for (uint32_t index = 0; index < entries; ++index) {
            uint32_t page = table * 1024U + index;
            system_rom_tables[table][index] =
                (boot_info->system_rom_base + page * MINEMU_PAGE_SIZE) |
                MINEMU_PTE_VALID | MINEMU_PTE_READABLE;
        }
        uint32_t directory_index = (boot_info->system_rom_base >> 22) + table;
        directory[directory_index] =
            kernel_vaddr_to_paddr(system_rom_tables[table]) | MINEMU_PDE_VALID;
    }
    minemu_mmu_tlbiall();
    return true;
}

static const struct minemu_module_record *find_module(const struct minemu_boot_info *boot_info,
                                                       const char *name, size_t name_length) {
    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;
    if (!image_records_valid(boot_info->module_table_offset, boot_info->module_count,
                             boot_info->image_size)) {
        return NULL;
    }

    const struct minemu_module_record *modules =
        (const struct minemu_module_record *)(image + boot_info->module_table_offset);
    for (uint32_t index = 0; index < boot_info->module_count; ++index) {
        const struct minemu_module_record *module = &modules[index];
        if (module->name_length == name_length &&
            image_range_valid(module->name_offset, module->name_length,
                              boot_info->image_size) &&
            bytes_equal(image + module->name_offset, name, name_length)) {
            return module;
        }
    }
    return NULL;
}

static uint32_t pte_flags_from_segment(uint32_t segment_flags) {
    uint32_t pte_flags = 0;
    if ((segment_flags & MINEMU_SEGMENT_READABLE) != 0) {
        pte_flags |= MINEMU_PTE_READABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_WRITABLE) != 0) {
        pte_flags |= MINEMU_PTE_WRITABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_EXECUTABLE) != 0) {
        pte_flags |= MINEMU_PTE_EXECUTABLE;
    }
    return pte_flags;
}

static int user_page_index(uint32_t page_vaddr, uint32_t flags, bool create) {
    for (size_t index = 0; index < user_page_count; ++index) {
        if (user_page_vaddrs[index] == page_vaddr) {
            if (create && user_page_flags[index] != flags) {
                return -1;
            }
            return (int)index;
        }
    }
    if (!create || user_page_count == MAX_USER_IMAGE_PAGES) {
        return -1;
    }
    size_t index = user_page_count++;
    user_page_vaddrs[index] = page_vaddr;
    user_page_flags[index] = flags;
    zero_bytes(user_image_pages[index], MINEMU_PAGE_SIZE);
    return (int)index;
}

static bool prepare_user_range(uint32_t vaddr, uint32_t length, uint32_t flags) {
    uint32_t end = vaddr + length;
    uint32_t page = vaddr & MINEMU_PTE_PAGE_MASK;
    while (page < end) {
        if (user_page_index(page, flags, true) < 0) {
            return false;
        }
        page += MINEMU_PAGE_SIZE;
    }
    return true;
}

static bool write_user_range(uint32_t vaddr, const uint8_t *source, uint32_t length) {
    while (length != 0) {
        uint32_t page_vaddr = vaddr & MINEMU_PTE_PAGE_MASK;
        int index = user_page_index(page_vaddr, 0, false);
        if (index < 0) {
            return false;
        }
        uint32_t offset = vaddr - page_vaddr;
        uint32_t chunk = MINEMU_PAGE_SIZE - offset;
        if (chunk > length) {
            chunk = length;
        }
        if (source == NULL) {
            zero_bytes(&user_image_pages[index][offset], chunk);
        } else {
            copy_bytes(&user_image_pages[index][offset], source, chunk);
            source += chunk;
        }
        vaddr += chunk;
        length -= chunk;
    }
    return true;
}

static bool load_user_module(const struct minemu_boot_info *boot_info,
                             const struct minemu_module_record *module) {
    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;
    if (module->segment_count == 0 ||
        !image_records_valid(module->segment_table_offset, module->segment_count,
                             boot_info->image_size) ||
        (module->entry_vaddr & 3U) != 0) {
        return false;
    }

    const struct minemu_module_segment *segments =
        (const struct minemu_module_segment *)(image + module->segment_table_offset);
    bool entry_is_executable = false;
    zero_bytes(user_page_table, sizeof(user_page_table));
    zero_bytes(user_stack, sizeof(user_stack));
    user_page_count = 0;

    for (uint32_t index = 0; index < module->segment_count; ++index) {
        const struct minemu_module_segment *segment = &segments[index];
        if (segment->memory_size == 0 || segment->file_size > segment->memory_size ||
            (segment->flags & ~(MINEMU_SEGMENT_READABLE | MINEMU_SEGMENT_WRITABLE |
                                MINEMU_SEGMENT_EXECUTABLE)) != 0 ||
            ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) != 0 &&
             (segment->flags & MINEMU_SEGMENT_READABLE) == 0) ||
            segment->virtual_address < USER_IMAGE_BASE ||
            segment->virtual_address >= USER_STACK_BOTTOM ||
            segment->memory_size > USER_STACK_BOTTOM - segment->virtual_address ||
            !image_range_valid(segment->data_offset, segment->file_size,
                               boot_info->image_size)) {
            return false;
        }

        uint32_t pte_flags = pte_flags_from_segment(segment->flags);
        if (!prepare_user_range(segment->virtual_address, segment->memory_size, pte_flags) ||
            !write_user_range(segment->virtual_address, NULL, segment->memory_size) ||
            !write_user_range(segment->virtual_address, image + segment->data_offset,
                              segment->file_size)) {
            return false;
        }

        if ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) != 0 &&
            segment->file_size >= 4U && module->entry_vaddr >= segment->virtual_address &&
            module->entry_vaddr - segment->virtual_address <= segment->file_size - 4U) {
            entry_is_executable = true;
        }
    }
    if (!entry_is_executable) {
        return false;
    }

    for (size_t index = 0; index < user_page_count; ++index) {
        uint32_t table_index = (user_page_vaddrs[index] >> 12) & 1023U;
        user_page_table[table_index] = kernel_vaddr_to_paddr(user_image_pages[index]) |
                                       MINEMU_PTE_VALID | MINEMU_PTE_USER |
                                       user_page_flags[index];
    }
    user_page_table[(USER_STACK_BOTTOM >> 12) & 1023U] =
        kernel_vaddr_to_paddr(user_stack) | MINEMU_PTE_VALID | MINEMU_PTE_USER |
        MINEMU_PTE_READABLE | MINEMU_PTE_WRITABLE;

    volatile uint32_t *directory = active_page_directory();
    directory[USER_IMAGE_BASE >> 22] =
        kernel_vaddr_to_paddr(user_page_table) | MINEMU_PDE_VALID;
    minemu_mmu_tlbiall();
    return true;
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    uint32_t mode = frame->spsr & CPSR_MODE_MASK;
    if (frame->exception_id != MINEMU_EXCEPTION_SVC || mode != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0) {
        frame->r[0] = (uint32_t)MINEMU_HELLO_SYSCALL_ERROR;
        return frame;
    }

    uint32_t instruction = *(const uint32_t *)(uintptr_t)frame->fault_pc;
    if (instruction != A32_SVC_ZERO || frame->r[7] != MINEMU_HELLO_SYSCALL_UART_PUTCHAR ||
        frame->r[0] > UINT8_MAX) {
        frame->r[0] = (uint32_t)MINEMU_HELLO_SYSCALL_ERROR;
        return frame;
    }

    if (!user_syscall_observed) {
        minemu_trace_event(TRACE_USER_SYSCALL);
        user_syscall_observed = true;
    }
    MINEMU_UART0->tx_data = frame->r[0];
    frame->r[0] = 0;
    return frame;
}

void minemu_undefined_dispatch(struct minemu_trap_frame *frame) {
    (void)frame;
    minemu_trace_event(UINT32_C(0x48454c03));
    minemu_fail_stop();
}

void minemu_abort_dispatch(struct minemu_trap_frame *frame) {
    (void)frame;
    minemu_trace_event(UINT32_C(0x48454c04));
    minemu_fail_stop();
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    static const char module_name[] = "user-hello";
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION || boot_info->size != sizeof(*boot_info) ||
        boot_info->direct_map_vaddr != MINEMU_KERNEL_DIRECT_BASE ||
        boot_info->direct_map_paddr != MINEMU_RAM_BASE ||
        boot_info->direct_map_size != MINEMU_KERNEL_DIRECT_SIZE ||
        !map_system_rom(boot_info)) {
        minemu_trace_event(UINT32_C(0x48454c01));
        minemu_fail_stop();
    }

    const struct minemu_module_record *module =
        find_module(boot_info, module_name, sizeof(module_name) - 1U);
    if (module == NULL || !load_user_module(boot_info, module)) {
        minemu_trace_event(UINT32_C(0x48454c02));
        minemu_fail_stop();
    }
    minemu_enter_user(module->entry_vaddr, USER_STACK_TOP);
}
