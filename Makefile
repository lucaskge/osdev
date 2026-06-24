ASM = nasm
CC = gcc
LD = ld

ASM_SRC = src/boot/boot.asm
ASM_OBJ = build/boot.o

# Adicionado build/mem.o na árvore de objetos
KERNEL_OBJS = build/kernel.o build/vga.o build/shell.o build/ata.o build/fs.o build/rtc.o build/task.o build/mem.o

LINKER_SCRIPT = linker.ld
KERNEL_BIN = targets/x86_64/iso/boot/kernel.bin
ISO_IMAGE = MeuOS.iso

.PHONY: all clean run

all: $(ISO_IMAGE)

$(ASM_OBJ): $(ASM_SRC)
	@mkdir -p build
	$(ASM) -f elf32 $(ASM_SRC) -o $(ASM_OBJ)

build/%.o: src/kernel/%.c
	@mkdir -p build
	$(CC) -m32 -ffreestanding -c $< -o $@

$(KERNEL_BIN): $(ASM_OBJ) $(KERNEL_OBJS) $(LINKER_SCRIPT)
	@mkdir -p targets/x86_64/iso/boot
	$(LD) -m elf_i386 -n -T $(LINKER_SCRIPT) -o $(KERNEL_BIN) $(ASM_OBJ) $(KERNEL_OBJS)

$(ISO_IMAGE): $(KERNEL_BIN) targets/x86_64/iso/boot/grub/grub.cfg
	grub-file --is-x86-multiboot2 $(KERNEL_BIN)
	grub-mkrescue -o $(ISO_IMAGE) targets/x86_64/iso

run: $(ISO_IMAGE)
	@dd if=/dev/zero of=build/disk.img bs=1M count=32 2>/dev/null
	qemu-system-x86_64 -cdrom $(ISO_IMAGE) -hda build/disk.img -m 512

clean:
	rm -rf build targets/x86_64/iso/boot/kernel.bin $(ISO_IMAGE)