#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <efi.h>
#include <efilib.h>

// Maximum number of fans supported
#define MAX_FANS 6

// Fan information structure
typedef struct {
    UINT8 index;              // 0-5
    CHAR16 label[16];         // "PCI", "PS", "EXHAUST", etc.
    UINT16 current_rpm;       // Current fan speed
    UINT16 target_rpm;        // Target speed (manual mode)
    UINT16 min_rpm;           // Minimum safe RPM
    UINT16 max_rpm;           // Maximum RPM
    BOOLEAN is_manual;        // TRUE if in manual mode
} FAN_INFO;

/**
 * Initialization and discovery
 */

// Initialize fan control system
EFI_STATUS fan_init(void);

// Discover all available fans and populate info array
EFI_STATUS fan_discover_all(FAN_INFO fans[], UINT8 *count);

/**
 * Fan reading functions
 */

// Read current fan RPM
EFI_STATUS fan_read_rpm(UINT8 fan_index, UINT16 *rpm);

// Read fan min/max RPM limits
EFI_STATUS fan_read_min_max(UINT8 fan_index, UINT16 *min_rpm, UINT16 *max_rpm);

// Check if fan is in manual mode
EFI_STATUS fan_get_mode(UINT8 fan_index, BOOLEAN *is_manual);

/**
 * Fan control functions
 */

// Set fan to manual or automatic mode
EFI_STATUS fan_set_manual_mode(UINT8 fan_index, BOOLEAN enable);

// Set target RPM (only in manual mode, value will be clamped to min/max)
EFI_STATUS fan_set_target_rpm(UINT8 fan_index, UINT16 rpm);

/**
 * Safety functions
 */

// Restore all fans to automatic mode
EFI_STATUS fan_restore_auto_mode_all(void);

#endif // FAN_CONTROL_H
