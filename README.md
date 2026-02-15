# Apple SMC Fan Control (UEFI)

A standalone UEFI application for controlling Apple SMC (System Management Controller) fans directly from the firmware environment, before the operating system loads.

## Features

- **Pre-boot Fan Control**: Control fans before OS loads
- **Direct SMC Communication**: Low-level I/O port access to SMC hardware
- **Interactive Text UI**: Simple console-based interface
- **Safety Features**:
  - Automatic RPM clamping to min/max limits
  - Auto-restore all fans to automatic mode on exit
  - Timeout protection for all SMC operations
- **Real-time Monitoring**: Live fan speed display
- **Per-fan Control**: Switch each fan between auto and manual modes independently

## Hardware Requirements

- **Apple Hardware**: Mac Pro, MacBook Pro, iMac, or other Mac with SMC
- **UEFI Firmware**: Modern Mac with EFI boot support
- **Platform**: x86_64 architecture

This has been tested on:
- Mac Pro (2009-2012) with 6 fans

## Software Requirements

### Build Dependencies

- **Arch Linux** (or similar distribution)
- **gnu-efi**: UEFI development headers and libraries
- **GCC**: C compiler
- **GNU binutils**: Linker and object tools

### Installation

```bash
# Install gnu-efi
sudo pacman -S gnu-efi

# Clone this repository
cd /home/macpro
git clone <repository-url> applesmc-efi
cd applesmc-efi

# Build
make
```

## Building

```bash
# Build the EFI application
make

# Clean build artifacts
make clean

# Show installation instructions
make install

# Show help
make help
```

The build process produces `applesmc.efi` - a PE32+ executable for EFI.

## Installation

### Step 1: Copy to EFI System Partition

```bash
# Create tools directory
sudo mkdir -p /boot/efi/EFI/tools

# Copy EFI application
sudo cp applesmc.efi /boot/efi/EFI/tools/
```

### Step 2: Boot to UEFI Shell

Reboot your Mac and enter the UEFI Shell (varies by model).

### Step 3: Run the Application

```
Shell> FS0:
FS0:\> cd EFI\tools
FS0:\EFI\tools\> applesmc.efi
```

## Usage

### Startup

When you run the application, it will:
1. Detect the Apple SMC
2. Initialize fan control
3. Discover all available fans
4. Display fan information

### Interactive Menu

The application provides an interactive text-based menu:

```
========================================
    Apple SMC Fan Control (UEFI)
========================================

Detected Fans:
 [0] PCI      : 2400 RPM (800-4500)  [AUTO]
 [1] PS       : 1200 RPM (600-2800)  [AUTO]
>[2] EXHAUST  : 1200 RPM (600-2800)  [MANUAL: 1500]
 [3] INTAKE   : 1200 RPM (600-2800)  [AUTO]
 [4] BOOSTA   : 3200 RPM (800-5200)  [AUTO]
 [5] BOOSTB   : 3200 RPM (800-5200)  [AUTO]

Commands:
  [0-5]  Select fan
  [a]    Set to Auto mode
  [m]    Set to Manual mode
  [+]    Increase RPM (+100)
  [-]    Decrease RPM (-100)
  [r]    Refresh display
  [q]    Quit

Select:
```

### Commands

- **0-5**: Select a fan by index
- **a**: Set selected fan to automatic mode (SMC firmware control)
- **m**: Set selected fan to manual mode
- **+**: Increase target RPM by 100 (only in manual mode)
- **-**: Decrease target RPM by 100 (only in manual mode)
- **r**: Refresh fan data from hardware
- **q**: Quit and restore all fans to automatic mode

### Workflow Example

1. Press `2` to select EXHAUST fan
2. Press `m` to switch to manual mode
3. Press `+` several times to increase RPM
4. Press `-` to decrease RPM
5. Press `a` to return to automatic mode
6. Press `q` to quit

## Safety Features

### Automatic RPM Clamping

All RPM values are automatically clamped to the fan's safe operating range:
- Minimum RPM (typically 600-800 RPM)
- Maximum RPM (typically 2800-5200 RPM depending on fan)

You cannot set unsafe values that could damage hardware.

### Auto-Restore on Exit

When you quit the application (press `q`), **all fans are automatically restored to automatic mode**. This ensures the SMC firmware resumes normal fan control even if you exit with fans in manual mode.

### Timeout Protection

All SMC I/O operations have 100ms timeouts to prevent infinite loops if the hardware hangs.

## Technical Details

### SMC Protocol

The application communicates with the Apple SMC via I/O ports:
- **Data Port**: 0x300 (read/write data)
- **Command Port**: 0x304 (commands and status)
- **Error Port**: 0x31e (error codes)

Commands:
- `0x10`: READ - Read SMC key value
- `0x11`: WRITE - Write SMC key value

### SMC Keys

Fan control keys follow the pattern `F[0-5][Ac|Mn|Mx|Md|Tg]`:
- `F0Ac` - Fan 0 actual RPM (read)
- `F0Mn` - Fan 0 minimum RPM (read)
- `F0Mx` - Fan 0 maximum RPM (read)
- `F0Md` - Fan 0 mode: 0=auto, 1=manual (read/write)
- `F0Tg` - Fan 0 target RPM (write in manual mode)

### fpe2 Encoding

RPM values are stored in fpe2 (fixed-point encoding, 2 decimal places):
- Encode: `value = rpm << 2`
- Decode: `rpm = value >> 2`
- Format: 2 bytes, big-endian

## Project Structure

```
applesmc-efi/
├── Makefile                # Build system
├── README.md               # This file
├── .gitignore              # Git ignore rules
├── src/
│   ├── main.c              # Application entry point
│   ├── smc_protocol.c/h    # SMC I/O protocol
│   ├── fan_control.c/h     # Fan control logic
│   ├── ui_menu.c/h         # Interactive UI
│   └── utils.c/h           # Utilities
├── test/
│   └── test_in_qemu.sh     # QEMU testing script
└── docs/
    └── SMC_PROTOCOL.md     # SMC protocol docs
```

## Related Projects

- **macsfancontrol-qt**: Qt5 Linux application for SMC fan control ([GitHub](https://github.com/startergo/macsfancontrol-qt))
- **smcFanControl**: macOS fan control application
- **QEMU applesmc**: SMC device emulation

## Troubleshooting

### "SMC not detected"

- Verify you're running on Apple hardware
- Check that applesmc kernel driver is available in Linux
- Some Mac models may have different SMC implementations

### "Failed to initialize fan control"

- SMC may be in use by another process
- Try rebooting and running immediately in UEFI

### Fans not responding

- Verify fan is in manual mode before adjusting RPM
- Check min/max RPM limits for the fan
- Some fans may have hardware restrictions

### Build errors

```bash
# Verify gnu-efi is installed
pacman -Q gnu-efi

# Check include paths
ls -la /usr/include/efi
ls -la /usr/lib/crt0-efi-x86_64.o
```

## Warning

**Use at your own risk!** Improper fan control can lead to:
- Overheating and hardware damage
- System instability
- Reduced component lifespan

Always:
- Monitor temperatures when testing
- Start with conservative RPM values
- Test one fan at a time
- Ensure adequate cooling during UEFI operations

The application includes safety features, but hardware damage is still possible with misuse.

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please:
1. Test thoroughly on your hardware
2. Document any Mac-specific quirks
3. Add support for additional SMC keys
4. Improve error handling

## Author

Built with assistance from Claude (Anthropic)

## Acknowledgments

- QEMU applesmc device implementation for protocol reference
- macsfancontrol project for SMC key mappings
- gnu-efi project for UEFI development tools
