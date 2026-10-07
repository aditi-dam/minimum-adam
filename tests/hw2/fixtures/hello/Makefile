PROGRAM := hw2-hello
MINEMU ?= minemu
ROOT := ../../../..
IMAGE := build/hw2-hello.img

include $(ROOT)/user/common/common.mk

.PHONY: kernel image

all: image

kernel:
	$(MAKE) -C $(ROOT)/kernel all

image: $(IMAGE)

$(IMAGE): image.toml $(ELF) kernel
	@mkdir -p $(dir $@)
	$(MINEMU) image image.toml --output $@
