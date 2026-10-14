# Local builds use the same toolchain images as CI.
IMAGE_n64 := ghcr.io/dragonminded/libdragon:preview
IMAGE_ogc := devkitpro/devkitppc:latest
IMAGE_nds := devkitpro/devkitarm:latest
IMAGE_3ds := devkitpro/devkitarm:latest

CONSOLES := n64 ogc nds 3ds

# Keep the N64 toolchain (with libdragon installed into it) in a named volume, so
# libdragon is only built once. `make n64-reset` drops it, e.g. after an image update.
N64_VOLUME := hangout64-n64-toolchain
DOCKER_ARGS_n64 := -v $(N64_VOLUME):/n64_toolchain

# Stop Git Bash on Windows rewriting the container paths (/src/...) into Windows paths.
export MSYS_NO_PATHCONV := 1

all: test pc $(CONSOLES)

test:
	$(MAKE) -C core test

pc:
	cmake -S platform/pc -B build/pc
	cmake --build build/pc

$(CONSOLES):
	docker run --rm $(DOCKER_ARGS_$@) -v "$(CURDIR):/src" -w /src/platform/$@ $(IMAGE_$@) make

clean:
	rm -rf build core/test_core
	$(foreach c,$(CONSOLES),docker run --rm $(DOCKER_ARGS_$(c)) -v "$(CURDIR):/src" -w /src/platform/$(c) $(IMAGE_$(c)) make clean;)

# Lua benchmark ROMs (bench/lua): measure Lua's cost on the slowest consoles.
bench-n64:
	docker run --rm $(DOCKER_ARGS_n64) -v "$(CURDIR):/src" -w /src/bench/lua $(IMAGE_n64) make -f Makefile.n64
bench-nds:
	docker run --rm -v "$(CURDIR):/src" -w /src/bench/lua $(IMAGE_nds) make -f Makefile.nds

n64-reset:
	docker volume rm $(N64_VOLUME)

.PHONY: all test pc clean n64-reset bench-n64 bench-nds $(CONSOLES)
