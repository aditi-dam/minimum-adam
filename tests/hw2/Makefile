MINEMU ?= minemu

.PHONY: all prepare-hello prepare-devices clean

all: prepare-hello prepare-devices

prepare-hello:
	$(MAKE) -C fixtures/hello MINEMU="$(MINEMU)" all

prepare-devices:
	$(MAKE) -C fixtures/devices MINEMU="$(MINEMU)" all

prepare-%:
	@:

clean:
	$(MAKE) -C fixtures/hello clean
	$(MAKE) -C fixtures/devices clean
