ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; use the devkitpro/devkitarm container)
endif

# Read by ds_rules' %.nds rule; must be set before the include.
GAME_TITLE     := hangout64
GAME_SUBTITLE1 := Spinning cube
GAME_SUBTITLE2 := github.com/gavinhsmith/hangout64

include $(DEVKITARM)/ds_rules

TARGET  := hangout64
BUILD   := build
OBJS    := $(BUILD)/main.o $(BUILD)/cube.o
ARCH    := -march=armv5te -mtune=arm946e-s
CFLAGS  := -O2 -Wall $(ARCH) -DARM9 -I$(LIBNDS)/include -I../core
LDFLAGS := -specs=ds_arm9.specs $(ARCH)
LIBS    := -L$(LIBNDS)/lib -lnds9 -lm

all: $(TARGET).nds

$(TARGET).nds: $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) *.elf *.nds

.PHONY: all clean
