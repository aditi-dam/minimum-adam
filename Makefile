MINEMU ?= minemu

.PHONY: all bootloader bootloader-check kernel kernel-examples user image \
	user-mode-hello-image clean

all: bootloader-check kernel user kernel-examples

bootloader:
	$(MAKE) -C bootloader all

bootloader-check:
	$(MAKE) -C bootloader check

kernel:
	$(MAKE) -C kernel all

kernel-examples:
	$(MAKE) -C examples kernel-examples

user:
	$(MAKE) -C user all

image: kernel user
	$(MAKE) -C image MINEMU="$(MINEMU)" all

user-mode-hello-image: bootloader-check
	$(MAKE) -C examples MINEMU="$(MINEMU)" user-mode-hello

clean:
	$(MAKE) -C bootloader clean
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean
	$(MAKE) -C image clean
	$(MAKE) -C examples clean
	@if [ -f tests/hw2/Makefile ]; then $(MAKE) -C tests/hw2 clean; fi
