# Makefile for Apple SMC Fan Control UEFI Application
# Requires gnu-efi package: sudo pacman -S gnu-efi

ARCH            = x86_64
EFILIB          = /usr/lib
EFICRT0         = $(EFILIB)/crt0-efi-$(ARCH).o
LDSCRIPT        = $(EFILIB)/elf_$(ARCH)_efi.lds
EFIINC          = /usr/include/efi
EFIINCARCH      = $(EFIINC)/$(ARCH)

TARGET          = applesmc.efi
OBJS            = main.o smc_protocol.o fan_control.o temp_sensors.o ui_menu.o utils.o

CC              = gcc
LD              = ld
OBJCOPY         = objcopy

CFLAGS          = -I$(EFIINC) -I$(EFIINCARCH) -fno-stack-protector \
                  -fpic -fshort-wchar -mno-red-zone -Wall -Wextra \
                  -DEFI_FUNCTION_WRAPPER -D_GNU_EFI -std=c11 -O2

LDFLAGS         = -nostdlib -znocombreloc -T $(LDSCRIPT) -shared \
                  -Bsymbolic -L$(EFILIB) $(EFICRT0)

LIBS            = -lefi -lgnuefi

.PHONY: all clean install help

all: $(TARGET)

%.o: src/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

applesmc.so: $(OBJS)
	@echo "Linking $@..."
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

%.efi: %.so
	@echo "Creating EFI binary $@..."
	$(OBJCOPY) --input-target=elf64-x86-64 --output-target=pe-x86-64 \
	           -j .text -j .sdata -j .data -j .dynamic \
	           -j .dynsym -j .rel -j .rela -j .reloc \
	           --subsystem=10 $< $@
	@echo ""
	@echo "Build successful!"
	@echo "Output: $(TARGET)"
	@file $(TARGET)

clean:
	@echo "Cleaning build artifacts..."
	rm -f *.o *.so $(TARGET)
	@echo "Clean complete."

install: $(TARGET)
	@echo ""
	@echo "To install, copy to your EFI System Partition:"
	@echo "  sudo mkdir -p /boot/efi/EFI/tools"
	@echo "  sudo cp $(TARGET) /boot/efi/EFI/tools/"
	@echo ""
	@echo "Then boot to UEFI Shell and run:"
	@echo "  FS0:\\EFI\\tools\\applesmc.efi"
	@echo ""

help:
	@echo "Apple SMC Fan Control - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      Build the UEFI application (default)"
	@echo "  clean    Remove build artifacts"
	@echo "  install  Show installation instructions"
	@echo "  help     Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  sudo pacman -S gnu-efi"
	@echo ""
	@echo "Build:"
	@echo "  make"
	@echo ""
