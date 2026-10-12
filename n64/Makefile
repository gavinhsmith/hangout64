BUILD_DIR = build
include $(N64_INST)/include/n64.mk

CFLAGS += -I../core

all: hangout64.z64

$(BUILD_DIR)/core/%.o: ../core/%.c
	@mkdir -p $(dir $@)
	$(N64_CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/hangout64.elf: $(BUILD_DIR)/main.o $(BUILD_DIR)/core/cube.o
hangout64.z64: N64_ROM_TITLE = "hangout64"

clean:
	rm -rf $(BUILD_DIR) *.z64

-include $(wildcard $(BUILD_DIR)/*.d $(BUILD_DIR)/core/*.d)

.PHONY: all clean
