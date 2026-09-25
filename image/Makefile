MINEMU ?= minemu

IMAGE := build/minimum.img
KERNEL := ../kernel/build/minimum-kernel.elf
USER_PROGRAM := ../user/prog/minimum-user/build/minimum-user.elf

KERNEL_INPUTS := $(wildcard ../kernel/src/core/*.c) \
	$(wildcard ../kernel/src/runtime/*.c) \
	$(wildcard ../kernel/src/startup/*.S) \
	$(wildcard ../kernel/src/core/*.S) \
	$(wildcard ../kernel/include/minemu/*.h) \
	../kernel/linker/kernel.ld \
	../kernel/Makefile

USER_INPUTS := $(wildcard ../user/prog/minimum-user/*.c) \
	$(wildcard ../user/prog/minimum-user/*.S) \
	$(wildcard ../user/lib/src/*.c) \
	$(wildcard ../user/lib/include/*.h) \
	../user/common/common.mk \
	../user/common/user.ld \
	../user/lib/Makefile \
	../user/prog/minimum-user/Makefile

.PHONY: all clean

all: $(IMAGE)

$(KERNEL): $(KERNEL_INPUTS)
	$(MAKE) -C ../kernel all

$(USER_PROGRAM): $(USER_INPUTS)
	$(MAKE) -C ../user/prog/minimum-user all

$(IMAGE): minimum.toml $(KERNEL) $(USER_PROGRAM)
	@mkdir -p $(dir $@)
	$(MINEMU) image minimum.toml --output $@

clean:
	rm -rf build
