MINEMU ?= minemu

IMAGE := build/user-mode-hello.img
KERNEL := kernel/build/user-mode-hello.elf
USER_PROGRAM := user/build/user-hello.elf

.PHONY: all kernel user image test clean

all: image

kernel:
	$(MAKE) -C kernel all

user:
	$(MAKE) -C user all

image: kernel user
	@mkdir -p $(dir $(IMAGE))
	$(MINEMU) image image.toml --output $(IMAGE)

test: image
	$(MINEMU) test test.toml

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean
	rm -rf build
