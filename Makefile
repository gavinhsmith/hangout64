# Local builds use the same toolchain images as CI.
IMAGE_n64 := ghcr.io/dragonminded/libdragon:preview
IMAGE_ogc := devkitpro/devkitppc:latest
IMAGE_nds := devkitpro/devkitarm:latest
IMAGE_3ds := devkitpro/devkitarm:latest

CONSOLES := n64 ogc nds 3ds

# Stop Git Bash on Windows rewriting the container paths (/src/...) into Windows paths.
export MSYS_NO_PATHCONV := 1

all: test pc $(CONSOLES)

test:
	$(MAKE) -C core test

pc:
	cmake -S pc -B build/pc
	cmake --build build/pc

$(CONSOLES):
	docker run --rm -v "$(CURDIR):/src" -w /src/$@ $(IMAGE_$@) make

clean:
	rm -rf build core/test_core
	$(foreach c,$(CONSOLES),docker run --rm -v "$(CURDIR):/src" -w /src/$(c) $(IMAGE_$(c)) make clean;)

.PHONY: all test pc clean $(CONSOLES)
