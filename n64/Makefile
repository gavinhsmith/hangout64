BUILD_DIR = build
include $(N64_INST)/include/n64.mk

CFLAGS += -I../core -MP  # -MP: a removed header must not break old .d files

all: hangout64.z64

$(BUILD_DIR)/core/%.o: ../core/%.c
	@mkdir -p $(dir $@)
	$(N64_CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/hangout64.elf: $(BUILD_DIR)/main.o $(BUILD_DIR)/core/world.o $(BUILD_DIR)/core/guy_mesh.o
hangout64.z64: N64_ROM_TITLE = "hangout64"

# The libdragon image ships only the toolchain. If libdragon isn't installed yet,
# make builds this include first (preview branch) and restarts.
$(N64_INST)/include/n64.mk:
	rm -rf /tmp/libdragon
	git clone --depth 1 --branch preview https://github.com/DragonMinded/libdragon /tmp/libdragon
	$(MAKE) -C /tmp/libdragon -j$$(nproc) install tools-install

clean:
	rm -rf $(BUILD_DIR) *.z64

-include $(wildcard $(BUILD_DIR)/*.d $(BUILD_DIR)/core/*.d)

.PHONY: all clean
