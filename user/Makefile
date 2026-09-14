BUILD_DIR ?= ../build/user
AS = fasm
LD = ld

CFLAGS = -m64 \
-ffreestanding \
-fno-stack-protector \
-fno-pic \
-fno-pie \
-mno-red-zone \
-I../include \
-g

LDFLAGS = -m elf_x86_64 \
-nostdlib \
-N

U_FILES = $(wildcard *.txt)
U_FILE_TARGETS = $(U_FILES:%=$(BUILD_DIR)/%) 

U_LIBS = $(BUILD_DIR)/usys.o $(BUILD_DIR)/ulib.o $(BUILD_DIR)/malloc.o 

U_PROGS = init nsh cat echo mkdir ls stress_fs

.PHONY: all clean

all: $(U_FILE_TARGETS) $(U_PROGS:%=$(BUILD_DIR)/%)

$(BUILD_DIR)/%.txt: %.txt
	@mkdir -p $(BUILD_DIR)
	cp $< $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# init_code.out is ELF
# $(BUILD_DIR)/init_code.out: $(BUILD_DIR)/init_code.o
# 	$(LD) $(LDFLAGS) -e _start -Ttext 0 -o $@ $<

# # init_code is just a binary file that could be embedded, flattened
# $(BUILD_DIR)/init_code: $(BUILD_DIR)/init_code.out
# 	objcopy -S -O binary $< $@

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(U_LIBS)
	$(LD) $(LDFLAGS) -e main -Ttext 0x1000 -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
