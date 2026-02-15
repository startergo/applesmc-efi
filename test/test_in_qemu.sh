#!/bin/bash
# Test applesmc.efi in QEMU with applesmc device emulation

set -e

echo "Apple SMC Fan Control - QEMU Test Script"
echo "========================================="
echo ""

# Check if applesmc.efi exists
if [ ! -f "../applesmc.efi" ]; then
    echo "ERROR: applesmc.efi not found"
    echo "Please build the application first: make"
    exit 1
fi

# Check dependencies
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: qemu-system-x86_64 not found"
    echo "Install with: sudo pacman -S qemu-system-x86"
    exit 1
fi

if ! command -v mcopy &> /dev/null; then
    echo "ERROR: mcopy not found (part of mtools)"
    echo "Install with: sudo pacman -S mtools"
    exit 1
fi

echo "Creating test environment..."

# Create test directories
mkdir -p test_env/esp/EFI/BOOT

# Copy EFI application as default boot loader
cp ../applesmc.efi test_env/esp/EFI/BOOT/BOOTX64.EFI

echo "Creating FAT disk image..."

# Create 100MB disk image
dd if=/dev/zero of=test_env/disk.img bs=1M count=100 2>/dev/null

# Format as FAT32
mkfs.vfat test_env/disk.img >/dev/null 2>&1

# Copy EFI directory structure
mcopy -i test_env/disk.img -s test_env/esp/EFI :: 2>/dev/null

echo "Starting QEMU..."
echo ""
echo "NOTE: QEMU applesmc device may show dummy values"
echo "Real hardware testing recommended for actual fan control"
echo ""
echo "Press Ctrl+Alt+G to release mouse if captured"
echo "Press Ctrl+C to quit QEMU"
echo ""

# Run QEMU with OVMF firmware and applesmc device
qemu-system-x86_64 \
    -enable-kvm \
    -m 2048 \
    -bios /usr/share/edk2/x64/OVMF_CODE.4m.fd \
    -drive file=test_env/disk.img,format=raw \
    -device isa-applesmc \
    -serial stdio \
    -nographic \
    -nodefaults \
    -vga none

echo ""
echo "QEMU session ended"
