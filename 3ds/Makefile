ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; use the devkitpro/devkitarm container)
endif

include $(DEVKITARM)/3ds_rules

TARGET  := hangout64
BUILD   := build
OBJS    := $(BUILD)/vshader.shbin.o $(BUILD)/main.o $(BUILD)/world.o $(BUILD)/guy_mesh.o
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS  := -O2 -Wall $(ARCH) -D__3DS__ -I$(CTRULIB)/include -I../core -I$(BUILD)
LDFLAGS := -specs=3dsx.specs $(ARCH)
LIBS    := -L$(CTRULIB)/lib -lcitro3d -lctru -lm

all: $(TARGET).3dsx

$(TARGET).3dsx: $(BUILD)/$(TARGET).elf
	3dsxtool $< $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

# Shader: assemble with picasso, embed with bin2s (also emits vshader_shbin.h).
$(BUILD)/vshader.shbin: vshader.v.pica | $(BUILD)
	picasso -o $@ $<

$(BUILD)/vshader.shbin.o: $(BUILD)/vshader.shbin
	cd $(BUILD) && bin2s -a 4 -H vshader_shbin.h vshader.shbin > vshader_shbin.s
	$(CC) $(ARCH) -c $(BUILD)/vshader_shbin.s -o $@

$(BUILD)/main.o: $(BUILD)/vshader.shbin.o

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) *.3dsx

.PHONY: all clean
