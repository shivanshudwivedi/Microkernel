# Microkernel OS Makefile
# Builds bootloader, kernel, and user programs for x86_64

# Compiler and tools
CC = gcc
AS = nasm
LD = ld
OBJCOPY = objcopy
QEMU = qemu-system-x86_64

# Flags
CFLAGS = -std=c11 -m64 -fno-stack-protector -fno-pie -no-pie -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -O2 -g
ASFLAGS = -f elf64
LDFLAGS = -m elf_x86_64 -T linker.ld
BOOTFLAGS = -f bin

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
LOADER_DIR = loader
USER_DIR = user
BUILD_DIR = build

# Source files
BOOT_SRC = $(BOOT_DIR)/boot.S
KERNEL_SRCS = $(KERNEL_DIR)/main.c $(KERNEL_DIR)/sched.c $(KERNEL_DIR)/ipc.c $(KERNEL_DIR)/vm.c
KERNEL_ASM = $(KERNEL_DIR)/context.S
LOADER_SRC = $(LOADER_DIR)/loader.c
USER_SRCS = $(wildcard $(USER_DIR)/*.c)

# Object files
BOOT_OBJ = $(BUILD_DIR)/boot.bin
KERNEL_OBJS = $(KERNEL_SRCS:.c=.o) $(KERNEL_ASM:.S=.o)
LOADER_OBJ = $(LOADER_SRC:.c=.o)
USER_OBJS = $(USER_SRCS:.c=.o)

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
INITRD_IMG = $(BUILD_DIR)/initrd.img

# Default target
all: $(BUILD_DIR) $(KERNEL_BIN) $(INITRD_IMG)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Bootloader
$(BOOT_OBJ): $(BOOT_SRC)
	$(AS) $(BOOTFLAGS) -o $@ $<

# Kernel object files
$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $<

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.S
	$(AS) $(ASFLAGS) -o $@ $<

# Loader object file
$(LOADER_OBJ): $(LOADER_SRC)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $<

# User program object files
$(USER_DIR)/%.o: $(USER_DIR)/%.c
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $<

# Kernel ELF
$(KERNEL_ELF): $(KERNEL_OBJS) $(LOADER_OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(LOADER_OBJ)

# Kernel binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# User programs
user_programs: $(USER_OBJS)
	@echo "Building user programs..."

# Initrd image
$(INITRD_IMG): user_programs
	@echo "Creating initrd image..."
	@mkdir -p $(BUILD_DIR)/initrd
	@cp $(USER_DIR)/hello_world $(BUILD_DIR)/initrd/
	@cp $(USER_DIR)/ipc_test $(BUILD_DIR)/initrd/ 2>/dev/null || true
	@cp $(USER_DIR)/page_test $(BUILD_DIR)/initrd/ 2>/dev/null || true
	cd $(BUILD_DIR) && tar -cf initrd.tar initrd/ && gzip -c initrd.tar > initrd.img

# Linker script
linker.ld:
	@echo "Creating linker script..."
	@echo 'ENTRY(kernel_main)' > linker.ld
	@echo 'SECTIONS {' >> linker.ld
	@echo '    . = 0x100000;' >> linker.ld
	@echo '    .text : { *(.text) }' >> linker.ld
	@echo '    .rodata : { *(.rodata) }' >> linker.ld
	@echo '    .data : { *(.data) }' >> linker.ld
	@echo '    .bss : { *(.bss) }' >> linker.ld
	@echo '}' >> linker.ld

# Run in QEMU
run: all
	$(QEMU) -kernel $(KERNEL_BIN) -initrd $(INITRD_IMG) -nographic -m 512M

# Run with GDB stub
debug: all
	$(QEMU) -kernel $(KERNEL_BIN) -initrd $(INITRD_IMG) -nographic -m 512M -s -S

# Clean
clean:
	rm -rf $(BUILD_DIR) $(KERNEL_OBJS) $(LOADER_OBJ) $(USER_OBJS) linker.ld

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y gcc nasm qemu-system-x86 gdb make

# Install dependencies (macOS)
install-deps-mac:
	brew install gcc nasm qemu gdb make

.PHONY: all clean run debug install-deps install-deps-mac user_programs 