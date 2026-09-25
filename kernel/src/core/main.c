#include <stddef.h>
#include <stdint.h>

#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"

#define INPUT_BUFFER_SIZE 256
#define MSH_LINE_MAX 20

static volatile uint8_t input_buffer[INPUT_BUFFER_SIZE];
static size_t input_head;
static size_t input_tail;

char line[MSH_LINE_MAX + 1];
size_t line_length = 0;
static int line_overflow;

static int input_getc(char *out)
{
    int available = 0;

    minemu_irq_disable();

    if (input_tail != input_head) {
        *out = input_buffer[input_head];
        input_head = (input_head + 1) % INPUT_BUFFER_SIZE;
        available = 1;
    }

    minemu_irq_enable();

    return available;
}


static void uart_putc(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }
    MINEMU_UART0->tx_data = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}


static void uart0_irq_handler(void)
{
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        uint8_t c = (uint8_t)MINEMU_UART0->rx_data;

        size_t next_tail = (input_tail + 1) % INPUT_BUFFER_SIZE;

        if (next_tail == input_head) {
            // char error[] = "Error";
            // uart_puts(error);
            minemu_fail_stop();   
        }

        input_buffer[input_tail] = c;
        input_tail = next_tail;
    }
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame)
{
    uint32_t source = (uint32_t)frame->exception_id;

    if (source == MINEMU_IRQ_UART0) {
        uart0_irq_handler();
    }

    MINEMU_INTERRUPT->eoi = source;

    return frame;
}


int echo_check(char *command) {
    if (command[0] == 'e' && 
        command[1] == 'c' && 
        command[2] == 'h' && 
        command[3] == 'o' && 
        command[4] == '\0') {
        return 1;
    }
    return 0;
}

static void msh_execute(char *line) {
    char command[MSH_LINE_MAX + 1] = {0};
    char text[MSH_LINE_MAX + 1] = {0};
    
    size_t i = 0;
    size_t command_index = 0;
    size_t text_index = 0;

    while (line[i] == ' ') {
        i++;
    }

    while (line[i] != ' ' && line[i] != '\0') {
        if (command_index < MSH_LINE_MAX) {
            command[command_index++] = line[i];
        }
        i++;
    }
    command[command_index] = '\0';

    while (line[i] == ' ') {
        i++;
    }

    while (line[i] != '\0') {
        if (text_index < MSH_LINE_MAX) {
            text[text_index++] = line[i];
        }
        i++;
    }
    text[text_index] = '\0';

    if (echo_check(command)) {
        if (text_index != 0) {
            uart_puts(text);
            uart_putc('\n');
        }
    }
    else {
        if (command_index != 0) {
            uart_puts("Command not found: ");
            uart_puts(command);
            uart_putc('\n');
        }
    }
}


void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }

    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable =
        UINT32_C(1) << MINEMU_IRQ_UART0;

    minemu_irq_enable();

    line_overflow = 0;
    uart_puts("msh> ");

    for (;;) {
        char c;
        
        if (input_getc(&c)) {
            if (c == '\n') {
                if (line_overflow) {
                    uart_puts("command too long\n");
                }
                else {
                    line[line_length] = '\0';
                    msh_execute(line);
                }
                
                for (size_t i = 0; i < sizeof(line); i++) {
                    line[i] = '\0';
                }
                line_length = 0;
                line_overflow = 0;

                uart_puts("msh> ");
            }
            else if (c == 8 || c == 127) {
                if (line_length > 0) {
                    line[--line_length] = '\0';
                }
            }
            else {
                line[line_length++] = c;
                if (line_length >= MSH_LINE_MAX+1) {
                    line_overflow = 1;
                }
            }
        }
    }
}