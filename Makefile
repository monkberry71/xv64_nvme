CC = gcc
LD = ld
AS = fasm

CFLAGS = -m64 \
-ffreestanding \
-fno-stack-protector \
-fno-pic \
-fno-pie \
-mno-red-zone \
-Iinclude \
-mcmodel=kernel

LDFLAGS = -m elf_x86_64 \
-nostdlib \
-T kernel/linker.ld

.PHONY: run clean debug format_usb format_esp 

C_SRCS = $(shell find kernel -name '*.c')
ASM_SRCS = $(shell find kernel -name '*.asm')
OBJS = $(patsubst kernel/%.c, build/%.o, $(C_SRCS)) $(patsubst kernel/%.asm, build/%.o, $(ASM_SRCS))

# Make ELFs

build/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< $@ 
# fasm is $< $@

build/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@


# Make GRUB
GRUB_MODULES = part_gpt fat normal multiboot2 all_video

build/BOOTX64.EFI: grub/grub.cfg
	@mkdir -p build
	grub-mkstandalone \
	--format=x86_64-efi \
	--output=$@ \
	--modules="$(GRUB_MODULES)" \
	"boot/grub/grub.cfg=grub/grub.cfg"

# Make Boot Images
build/esp.img:
	@mkdir -p build
	dd if=/dev/zero of=build/esp.img bs=1M count=64
# 64MB 

#PHONY
format_esp: build/esp.img build/BOOTX64.EFI build/kernel.elf
	mformat -i build/esp.img -F -T 131072 -H 2048 ::
# 	64MB / (1SEC=512B) -> 2^26 / 2^9 -> 2^17 -> 131072
#   Size Sector Count is 131072
#   GPT got 1MB, 1MB / (1SEC=512B) -> 2^20 / 2^9 -> 2^11 -> 2048
#	GPT start offset is 2048
	mmd -i build/esp.img ::/EFI ::/EFI/BOOT ::/boot
	mcopy -i build/esp.img build/BOOTX64.EFI ::/EFI/BOOT/
	mcopy -i build/esp.img build/kernel.elf ::/boot/

build/usb.img:
	@mkdir -p build
	dd if=/dev/zero of=$@ bs=1M count=128
#   128MB image

build/fs.img:
	@mkdir -p build
	dd if=/dev/zero of=$@ bs=1M count=128

#PHONY
format_usb: build/usb.img format_esp
	parted -s $< mklabel gpt
	parted -s $< mkpart ESP fat32 1MiB 65MiB
	parted -s $< set 1 esp on

	dd if=build/esp.img of=$< bs=512 seek=2048 conv=notrunc

OVMF = /usr/share/ovmf/OVMF.fd
SMP ?= 4

#PHONY
run: format_usb build/fs.img
	qemu-system-x86_64 \
	-machine q35 \
	-drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
	-drive format=raw,file=build/usb.img \
	-drive if=none,id=nvme0,format=raw,file=build/fs.img \
	-device nvme,drive=nvme0,serial=deadbeef,logical_block_size=4096,physical_block_size=4096 \
	-m 512M \
	-smp $(SMP) \
	-vga std \
	-serial stdio \
	-d int,cpu_reset -D ./misc/qemu.log \
	-monitor vc \

#PHONY
debug: format_usb
	qemu-system-x86_64 \
	-drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
	-drive format=raw,file=build/usb.img \
	-m 128M \
	-smp $(SMP) \
	-vga std \
	-serial stdio \
	-monitor vc \
	-s -S \
	-no-reboot \
	-d int,cpu_reset -D ./misc/qemu.log \
	-accel tcg

#PHONY
clean:
	rm -rf build/