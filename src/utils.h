#ifndef UTILS_H
#define UTILS_H

// Support both gnu-efi and EDK2/TianoCore build systems
#ifdef _GNU_EFI
  #include <efi.h>
  #include <efilib.h>
#else
  #include <Uefi.h>
  #include <Library/UefiLib.h>
  #include <Library/PrintLib.h>
  #include <Library/UefiBootServicesTableLib.h>
#endif

/**
 * Delay functions
 */

// Delay for specified microseconds
void delay_microseconds(UINT32 us);

// Delay for specified milliseconds
void delay_milliseconds(UINT32 ms);

/**
 * Value clamping and validation
 */

// Clamp RPM value to safe range
UINT16 clamp_rpm(UINT16 rpm, UINT16 min, UINT16 max);

/**
 * String/formatting utilities
 */

// Format fan label from index
void format_fan_label(UINT8 index, CHAR16 *label, UINTN label_size);

// Copy ASCII string to CHAR16 wide string
void ascii_to_wide(const CHAR8 *src, CHAR16 *dest, UINTN dest_size);

/**
 * fpe2 encoding/decoding for RPM values
 * fpe2 = fixed-point encoding with 2 decimal places
 */

// Encode RPM to fpe2 format (2 bytes)
void encode_fpe2(UINT16 rpm, UINT8 *bytes);

// Decode fpe2 format to RPM
UINT16 decode_fpe2(const UINT8 *bytes);

#endif // UTILS_H
