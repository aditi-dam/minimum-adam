#include <stdint.h>
#include <stddef.h>

#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/user_abi.h"

#define MSH_LINE_MAX 25
#define INPUT_BUFFER_SIZE 16

static char input_buffer[INPUT_BUFFER_SIZE];
static size_t input_pos = 0;
static size_t input_len = 0;

char line[MSH_LINE_MAX + 1];
size_t line_length = 0;
static int line_overflow;

static int input_getc(char *out)
{
    if (input_pos >= input_len) {
        int result = ioctl(MINEMU_FD_UART_OUT,
                           MINEMU_IOCTL_UART_READ,
                           input_buffer,
                           INPUT_BUFFER_SIZE);

        if (result <= 0) {
            return 0;
        }

        input_pos = 0;
        input_len = (size_t)result;
    }

    *out = input_buffer[input_pos++];
    return 1;
}


static void uart_putc(char c) {
     (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, &c, 1);
}

static void uart_puts(const char *s) {
    size_t length = 0;
    while (s[length] != '\0') {
        length++;
    }

    (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)s, length);
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

    // Initial whitespace
    while (line[i] == ' ') {
        i++;
    }

    // Command
    while (line[i] != ' ' && line[i] != '\0') {
        if (command_index < MSH_LINE_MAX) {
            command[command_index++] = line[i];
        }
        i++;
    }
    command[command_index] = '\0';

    // Whitespace between command and rest of message
    while (line[i] == ' ') {
        i++;
    }

    // Rest of message
    int prev_was_space = 0;
    while (line[i] != '\0') {
        if (line[i] == ' ') {
            if (!prev_was_space && text_index < MSH_LINE_MAX) {
                text[text_index++] = line[i];
            }
            prev_was_space = 1;
        }
        else {
            if (text_index < MSH_LINE_MAX) {
                text[text_index++] = line[i];
            }
            prev_was_space = 0;
        }
        i++;
    }

    // Trailing space removal
    if (text_index > 0 && text[text_index - 1] == ' ') {
        text_index--;
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
            uart_puts("command not found: ");
            uart_puts(command);
            uart_putc('\n');
        }
    }
}


void minemu_user_main(void) {
    line_overflow = 0;
    uart_puts("msh> ");

    for (;;) {
        char c;
        
        if (input_getc(&c)) {
            if (c == '\n') {
                if (!line_overflow) {
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
                if (line_length < MSH_LINE_MAX) {
                    line[line_length++] = c;
                } else {
                    line_overflow = 1;
                }
            }
        }
    }

    for (;;) {
        __asm__ volatile("nop");
    }
}