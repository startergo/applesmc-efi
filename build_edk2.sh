#!/bin/bash
# Build script for EDK2/TianoCore

set -e

# Configuration
EDK2_PATH="${EDK2_PATH:-/home/macpro/edk2}"
PROJECT_PATH="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="ApplesSmcEfiPkg"

echo "========================================="
echo "  Apple SMC Fan Control - EDK2 Build"
echo "========================================="
echo ""
echo "EDK2 Path: $EDK2_PATH"
echo "Project Path: $PROJECT_PATH"
echo ""

# Check if EDK2 exists
if [ ! -d "$EDK2_PATH" ]; then
    echo "Error: EDK2 not found at $EDK2_PATH"
    echo ""
    echo "To install EDK2:"
    echo "  cd /home/macpro"
    echo "  git clone https://github.com/tianocore/edk2.git"
    echo "  cd edk2"
    echo "  git submodule update --init"
    echo "  make -C BaseTools"
    echo ""
    exit 1
fi

# Check if BaseTools are built
if [ ! -f "$EDK2_PATH/BaseTools/Source/C/bin/GenFw" ]; then
    echo "Error: EDK2 BaseTools not built"
    echo ""
    echo "To build BaseTools:"
    echo "  cd $EDK2_PATH"
    echo "  make -C BaseTools"
    echo ""
    exit 1
fi

# Source EDK2 environment
echo "Setting up EDK2 environment..."
cd "$EDK2_PATH"
export WORKSPACE=$EDK2_PATH
export PACKAGES_PATH=$EDK2_PATH
source edksetup.sh BaseTools > /dev/null 2>&1

# Link project into EDK2 workspace if not already linked
if [ ! -e "$EDK2_PATH/$PROJECT_NAME" ]; then
    echo "Creating symbolic link: $EDK2_PATH/$PROJECT_NAME -> $PROJECT_PATH"
    ln -s "$PROJECT_PATH" "$EDK2_PATH/$PROJECT_NAME"
else
    # Check if existing link is correct
    LINK_TARGET=$(readlink -f "$EDK2_PATH/$PROJECT_NAME" 2>/dev/null || echo "")
    if [ "$LINK_TARGET" != "$PROJECT_PATH" ]; then
        echo "Warning: $EDK2_PATH/$PROJECT_NAME exists but points to different location"
        echo "  Current: $LINK_TARGET"
        echo "  Expected: $PROJECT_PATH"
        echo ""
        read -p "Remove and recreate link? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -f "$EDK2_PATH/$PROJECT_NAME"
            ln -s "$PROJECT_PATH" "$EDK2_PATH/$PROJECT_NAME"
        else
            echo "Aborting."
            exit 1
        fi
    fi
fi

# Determine build type
BUILD_TYPE="${1:-RELEASE}"
if [ "$BUILD_TYPE" != "DEBUG" ] && [ "$BUILD_TYPE" != "RELEASE" ]; then
    echo "Invalid build type: $BUILD_TYPE"
    echo "Usage: $0 [DEBUG|RELEASE]"
    exit 1
fi

echo ""
echo "Building $BUILD_TYPE configuration..."
echo ""

# Build
build -a X64 -t GCC5 -p "$PROJECT_NAME/ApplesSmcEfi.dsc" -b "$BUILD_TYPE"

# Determine output path
OUTPUT_DIR="$EDK2_PATH/Build/ApplesSmcEfi/${BUILD_TYPE}_GCC5/X64"
OUTPUT_EFI="$OUTPUT_DIR/ApplesSmcEfi.efi"

# Check if build succeeded
if [ ! -f "$OUTPUT_EFI" ]; then
    echo ""
    echo "Error: Build failed - output file not found"
    echo "Expected: $OUTPUT_EFI"
    exit 1
fi

# Copy to project directory
cp "$OUTPUT_EFI" "$PROJECT_PATH/ApplesSmcEfi.efi"

# Display build info
echo ""
echo "========================================="
echo "  Build Successful!"
echo "========================================="
echo ""
echo "Output file: $PROJECT_PATH/ApplesSmcEfi.efi"
file "$PROJECT_PATH/ApplesSmcEfi.efi"
ls -lh "$PROJECT_PATH/ApplesSmcEfi.efi"
echo ""
echo "To install:"
echo "  sudo mkdir -p /boot/efi/EFI/tools"
echo "  sudo cp ApplesSmcEfi.efi /boot/efi/EFI/tools/"
echo ""
echo "To run from UEFI Shell:"
echo "  FS0:\\EFI\\tools\\ApplesSmcEfi.efi"
echo ""
