KERNEL_EXAMPLES := \
	mmio-basics \
	svc-context-switch \
	irq-context-switch

.PHONY: all kernel-examples user-mode-hello clean $(KERNEL_EXAMPLES)

all: kernel-examples

kernel-examples: $(KERNEL_EXAMPLES)

$(KERNEL_EXAMPLES):
	$(MAKE) -C $@ all

user-mode-hello:
	$(MAKE) -C user-mode-hello image

clean:
	@for example in $(KERNEL_EXAMPLES); do $(MAKE) -C $$example clean; done
	$(MAKE) -C user-mode-hello clean
