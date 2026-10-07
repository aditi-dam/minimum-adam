ARM_PREFIX ?= arm-none-eabi-
CC := $(ARM_PREFIX)gcc

EXAMPLES_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(abspath $(EXAMPLES_DIR)/../kernel)
PROGRAM ?= $(notdir $(CURDIR))
BUILD := build
ELF := $(BUILD)/$(PROGRAM).elf
SOURCES_C := $(wildcard *.c)
SOURCES_S := $(wildcard *.S)
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) \
	$(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))
DEPENDENCIES := $(OBJECTS:.o=.d)
KERNEL_RUNTIME := $(KERNEL_DIR)/build/libminemu_kernel.a
KERNEL_RUNTIME_INPUTS := $(wildcard $(KERNEL_DIR)/src/runtime/*.c) \
	$(wildcard $(KERNEL_DIR)/src/startup/*.S) \
	$(wildcard $(KERNEL_DIR)/include/minemu/*.h) $(KERNEL_DIR)/Makefile

ARCH_FLAGS := -mcpu=cortex-a9 -marm -mfloat-abi=soft
CPPFLAGS := -I$(KERNEL_DIR)/include
CFLAGS := $(ARCH_FLAGS) -ffreestanding -fno-builtin -fdata-sections \
	-ffunction-sections -fno-stack-protector -std=c11 -Wall -Wextra -Werror -MMD -MP
ASFLAGS := $(ARCH_FLAGS) -ffreestanding -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -nostdlib -Wl,-T,$(KERNEL_DIR)/linker/kernel.ld \
	-Wl,--gc-sections -Wl,-u,minemu_bootstrap -Wl,-Map,$(BUILD)/$(PROGRAM).map
LIBGCC := $(shell $(CC) $(ARCH_FLAGS) -print-libgcc-file-name)

.PHONY: all clean

all: $(ELF)

$(KERNEL_RUNTIME): $(KERNEL_RUNTIME_INPUTS)
	$(MAKE) -C $(KERNEL_DIR) runtime

$(ELF): $(OBJECTS) $(KERNEL_RUNTIME) $(KERNEL_DIR)/linker/kernel.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJECTS) $(KERNEL_RUNTIME) $(LIBGCC) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)
