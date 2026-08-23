BUILD_DIR ?= ../build/user
AS = fasm
LD = ld

U_FILES = $(wildcard *.txt)
U_FILE_TARGETS = $(U_FILES:%=$(BUILD_DIR)/%) 

U_ASM_SRCS = $(wildcard *.asm)
U_ASM_OBJS = $(U_ASM_SRCS:%.asm=$(BUILD_DIR)/%.o)
U_ASM_TARGETS = $(U_ASM_SRCS:%.asm=$(BUILD_DIR)/%)

.PHONY: all clean

all: $(U_FILE_TARGETS) $(U_ASM_TARGETS)

$(BUILD_DIR)/%.txt: %.txt
	@mkdir -p $(BUILD_DIR)
	cp $< $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $< $@

# init_code.out is ELF
$(BUILD_DIR)/init_code.out: $(BUILD_DIR)/init_code.o
	$(LD) -m elf_x86_64 -nostdlib -N -e _start -Ttext 0 -o $@ $<

# init_code is just a binary file that could be embedded, flattened
$(BUILD_DIR)/init_code: $(BUILD_DIR)/init_code.out
	objcopy -S -O binary $< $@

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o
	$(LD) -m elf_x86_64 -nostdlib -Ttext-segment 0x1000 $< -o $@

clean:
	rm -rf $(BUILD_DIR)
