# Building with EDK2/TianoCore

This document describes how to build the Apple SMC Fan Control application using the EDK2/TianoCore build system as an alternative to gnu-efi.

## Prerequisites

### Install EDK2

```bash
# Clone EDK2 repository
cd /home/macpro
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init

# Build BaseTools
make -C BaseTools

# Set up build environment
source edksetup.sh
```

### Install Build Dependencies

```bash
# Arch Linux
sudo pacman -S base-devel nasm iasl python

# Ubuntu/Debian
sudo apt-get install build-essential nasm iasl python3
```

## Integration into EDK2

### Method 1: As a Package in EDK2 Tree

```bash
# Create symbolic link in EDK2 workspace
cd /home/macpro/edk2
ln -s /home/macpro/applesmc-efi ApplesSmcEfiPkg

# Edit Conf/target.txt to set:
#   ACTIVE_PLATFORM = ApplesSmcEfiPkg/ApplesSmcEfi.dsc
#   TARGET_ARCH = X64
#   TOOL_CHAIN_TAG = GCC5
```

Create `ApplesSmcEfi.dsc` in the project root:

```ini
[Defines]
  PLATFORM_NAME                  = ApplesSmcEfi
  PLATFORM_GUID                  = 6C5E4D3B-2A1F-4E9D-8C7A-0B1C2D3E4F5A
  PLATFORM_VERSION               = 1.0
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/ApplesSmcEfi
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf

[Components]
  ApplesSmcEfiPkg/ApplesSmcEfi.inf
```

### Method 2: Standalone Build

Create a standalone build script `build_edk2.sh`:

```bash
#!/bin/bash
set -e

EDK2_PATH="/home/macpro/edk2"
PROJECT_PATH="/home/macpro/applesmc-efi"

# Source EDK2 environment
cd $EDK2_PATH
source edksetup.sh

# Link project if not already linked
if [ ! -L "$EDK2_PATH/ApplesSmcEfiPkg" ]; then
    ln -s $PROJECT_PATH $EDK2_PATH/ApplesSmcEfiPkg
fi

# Copy DSC file if not present
if [ ! -f "$PROJECT_PATH/ApplesSmcEfi.dsc" ]; then
    echo "Error: ApplesSmcEfi.dsc not found"
    exit 1
fi

# Build
build -a X64 -t GCC5 -p ApplesSmcEfiPkg/ApplesSmcEfi.dsc -b RELEASE

# Copy output
cp Build/ApplesSmcEfi/RELEASE_GCC5/X64/ApplesSmcEfi.efi $PROJECT_PATH/

echo ""
echo "Build successful!"
echo "Output: $PROJECT_PATH/ApplesSmcEfi.efi"
```

## Building

### Using EDK2 Build Command

```bash
cd /home/macpro/edk2
source edksetup.sh

# Debug build
build -a X64 -t GCC5 -p ApplesSmcEfiPkg/ApplesSmcEfi.dsc -b DEBUG

# Release build
build -a X64 -t GCC5 -p ApplesSmcEfiPkg/ApplesSmcEfi.dsc -b RELEASE
```

### Using Standalone Script

```bash
cd /home/macpro/applesmc-efi
chmod +x build_edk2.sh
./build_edk2.sh
```

## Output Location

EDK2 build output will be located at:
```
/home/macpro/edk2/Build/ApplesSmcEfi/RELEASE_GCC5/X64/ApplesSmcEfi.efi
```

## Differences from gnu-efi Build

| Aspect | gnu-efi | EDK2 |
|--------|---------|------|
| Build System | Make | EDK2 BaseTools |
| Headers | `/usr/include/efi` | EDK2 MdePkg |
| Libraries | libgnuefi, libefi | EDK2 library framework |
| Entry Point | `efi_main` | `UefiMain` |
| I/O Access | Inline assembly | IoLib (or inline assembly) |
| Output | Direct PE32+ conversion | Native PE32+ |
| Size | ~76 KB | Variable (typically larger) |

## Advantages of EDK2 Build

- **Native UEFI Development**: EDK2 is the official UEFI reference implementation
- **Better Integration**: Easier to integrate with other EDK2 modules
- **More Libraries**: Access to full EDK2 library ecosystem
- **Better Debugging**: EDK2 has better debugging support
- **Standards Compliance**: Ensures strict UEFI specification compliance

## Advantages of gnu-efi Build

- **Simplicity**: Standard Makefile, no complex build system
- **Smaller Binaries**: More compact output files
- **Faster Builds**: Quicker compilation and linking
- **Less Dependencies**: Only needs gnu-efi package
- **Easier Development**: Simpler for quick iteration

## Troubleshooting

### Build Errors

**Error: "No such file or directory"**
- Ensure EDK2 submodules are initialized: `git submodule update --init`
- Check that BaseTools are built: `make -C BaseTools`

**Error: "TOOL_CHAIN_TAG not set"**
- Run `source edksetup.sh` in EDK2 directory
- Edit `Conf/target.txt` to set TOOL_CHAIN_TAG

**Error: "Cannot find library class"**
- Check that all library classes in .dsc match those in ApplesSmcEfi.inf
- Verify MdePkg and MdeModulePkg are present in EDK2

### Inline Assembly Issues

If inline assembly causes issues with EDK2 build, consider using IoLib:

```c
// Instead of inline assembly in smc_protocol.c:
#include <Library/IoLib.h>

UINT8 smc_inb(UINT16 port) {
    return IoRead8(port);
}

void smc_outb(UINT16 port, UINT8 value) {
    IoWrite8(port, value);
}
```

## Testing

Both build outputs (gnu-efi and EDK2) should produce functionally identical applications:

```bash
# Copy to ESP
sudo cp ApplesSmcEfi.efi /boot/efi/EFI/tools/

# Test in UEFI Shell
Shell> FS0:\EFI\tools\ApplesSmcEfi.efi
```

## Recommendation

For most users, **gnu-efi** is recommended because:
- Simpler build process
- Faster compilation
- Smaller binary size
- Adequate for this application's needs

Use **EDK2** if you need:
- Integration with other EDK2 drivers
- Strict UEFI specification compliance
- Advanced EDK2 library features
- Corporate/enterprise development standards
