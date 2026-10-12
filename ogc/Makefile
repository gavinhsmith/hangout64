# Builds both hangout64_gamecube.dol and hangout64_wii.dol from the same source.
ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC is not set; use the devkitpro/devkitppc container)
endif

ifeq ($(PLATFORM),)

all:
	$(MAKE) PLATFORM=gamecube
	$(MAKE) PLATFORM=wii

clean:
	rm -rf build *.dol

.PHONY: all clean

else

include $(DEVKITPPC)/$(PLATFORM)_rules

TARGET := hangout64_$(PLATFORM)
BUILD  := build/$(PLATFORM)
OBJS   := $(BUILD)/main.o $(BUILD)/cube.o
CFLAGS := -O2 -Wall $(MACHDEP) -I$(LIBOGC_INC) -I../core
LIBS   := -L$(LIBOGC_LIB) $(if $(filter wii,$(PLATFORM)),-lwiiuse -lbte) -logc -lm

.DEFAULT_GOAL := $(TARGET).dol

$(TARGET).dol: $(BUILD)/$(TARGET).elf
	elf2dol $< $@
	python3 dolpad.py $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(MACHDEP) $^ $(LIBS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: ../core/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

endif
