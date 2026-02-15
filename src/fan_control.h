#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

// Support both gnu-efi and EDK2/TianoCore build systems
#ifdef _GNU_EFI
  #include <efi.h>
  #include <efilib.h>
#else
  #include <Uefi.h>
  #include <Library/UefiLib.h>
#endif

// Maximum number of fans supported
#define MAX_FANS 6

// Fan control modes
typedef enum {
    FAN_MODE_AUTO = 0,           // Automatic (SMC firmware control)
    FAN_MODE_MANUAL = 1,         // Manual (fixed RPM)
    FAN_MODE_SENSOR_BASED = 2    // Sensor-based (automatic based on temperature)
} FAN_MODE;

// Fan information structure
typedef struct {
    UINT8 index;              // 0-5
    CHAR16 label[16];         // "PCI", "PS", "EXHAUST", etc.
    UINT16 current_rpm;       // Current fan speed
    UINT16 target_rpm;        // Target speed (manual/sensor mode)
    UINT16 min_rpm;           // Minimum safe RPM
    UINT16 max_rpm;           // Maximum RPM
    FAN_MODE mode;            // Current operating mode

    // Sensor-based control settings
    BOOLEAN sensor_based_enabled;  // TRUE if sensor-based control active
    UINT8 sensor_index;            // Index of temperature sensor to use
    INT16 min_temp;                // Minimum temperature (decidegrees C)
    INT16 max_temp;                // Maximum temperature (decidegrees C)
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
 * Sensor-based control functions
 */

// Enable/disable sensor-based control for a fan
EFI_STATUS fan_set_sensor_based_mode(UINT8 fan_index, BOOLEAN enable,
                                     UINT8 sensor_index, INT16 min_temp, INT16 max_temp);

// Update fan speed based on current temperature (call periodically in sensor-based mode)
EFI_STATUS fan_update_sensor_based(FAN_INFO *fan, INT16 current_temp);

// Calculate target RPM based on temperature and thresholds
UINT16 fan_calculate_rpm_from_temp(INT16 current_temp, INT16 min_temp, INT16 max_temp,
                                    UINT16 min_rpm, UINT16 max_rpm);

/**
 * Safety functions
 */

// Restore all fans to automatic mode
EFI_STATUS fan_restore_auto_mode_all(void);

#endif // FAN_CONTROL_H
