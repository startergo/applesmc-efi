#include "utils.h"

// Fan label names (same as Linux Qt app)
static const CHAR16 *fan_labels[] = {
    L"PCI",
    L"PS",
    L"EXHAUST",
    L"INTAKE",
    L"BOOSTA",
    L"BOOSTB"
};

#define FAN_COUNT 6

/**
 * Delay for specified microseconds using UEFI Boot Services
 */
void delay_microseconds(UINT32 us) {
    gBS->Stall(us);
}

/**
 * Delay for specified milliseconds
 */
void delay_milliseconds(UINT32 ms) {
    // Convert milliseconds to microseconds
    gBS->Stall(ms * 1000);
}

/**
 * Clamp RPM value to safe range
 * Ensures RPM is never below min or above max
 */
UINT16 clamp_rpm(UINT16 rpm, UINT16 min, UINT16 max) {
    if (rpm < min) {
        return min;
    }
    if (rpm > max) {
        return max;
    }
    return rpm;
}

/**
 * Format fan label from index
 */
void format_fan_label(UINT8 index, CHAR16 *label, UINTN label_size) {
    if (!label || label_size == 0) {
        return;
    }

    if (index < FAN_COUNT) {
        // Copy predefined label
        UINTN i;
        const CHAR16 *src = fan_labels[index];
        for (i = 0; i < label_size - 1 && src[i] != L'\0'; i++) {
            label[i] = src[i];
        }
        label[i] = L'\0';
    } else {
        // Unknown fan, use generic label
        UnicodeSPrint(label, label_size, L"FAN%d", index);
    }
}

/**
 * Copy ASCII string to wide character string
 */
void ascii_to_wide(const CHAR8 *src, CHAR16 *dest, UINTN dest_size) {
    UINTN i;

    if (!src || !dest || dest_size == 0) {
        return;
    }

    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = (CHAR16)src[i];
    }
    dest[i] = L'\0';
}

/**
 * Encode RPM to fpe2 format (fixed-point encoding with 2 decimal places)
 * Formula: value = rpm << 2
 * Result is 2 bytes in big-endian format
 */
void encode_fpe2(UINT16 rpm, UINT8 *bytes) {
    if (!bytes) {
        return;
    }

    // Shift left by 2 for fpe2 format
    UINT16 value = rpm << 2;

    // Store as big-endian (MSB first)
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
}

/**
 * Decode fpe2 format to RPM
 * Formula: rpm = value >> 2
 */
UINT16 decode_fpe2(const UINT8 *bytes) {
    if (!bytes) {
        return 0;
    }

    // Reconstruct 16-bit value from big-endian bytes
    UINT16 value = (bytes[0] << 8) | bytes[1];

    // Shift right by 2 to get actual RPM
    return value >> 2;
}
