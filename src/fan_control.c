#include "fan_control.h"
#include "smc_protocol.h"
#include "utils.h"

// SMC key suffixes for fan control
#define KEY_ACTUAL_RPM  "Ac"  // Actual RPM (read)
#define KEY_MIN_RPM     "Mn"  // Minimum RPM (read)
#define KEY_MAX_RPM     "Mx"  // Maximum RPM (read)
#define KEY_MODE        "Md"  // Mode (0=auto, 1=manual)
#define KEY_TARGET_RPM  "Tg"  // Target RPM (write in manual mode)

// Build SMC key for fan operation
// Format: F[0-5][Ac|Mn|Mx|Md|Tg]
static void build_fan_key(UINT8 fan_index, const CHAR8 *suffix, CHAR8 key[4]) {
    key[0] = 'F';
    key[1] = '0' + fan_index;
    key[2] = suffix[0];
    key[3] = suffix[1];
}

/**
 * Initialize fan control system
 */
EFI_STATUS fan_init(void) {
    // Verify SMC is accessible
    return smc_init();
}

/**
 * Read current fan RPM
 */
EFI_STATUS fan_read_rpm(UINT8 fan_index, UINT16 *rpm) {
    CHAR8 key[4];
    UINT8 data[32];
    UINT8 data_len = 0;
    EFI_STATUS status;

    if (fan_index >= MAX_FANS || !rpm) {
        return EFI_INVALID_PARAMETER;
    }

    // Build key F[n]Ac
    build_fan_key(fan_index, KEY_ACTUAL_RPM, key);

    // Read SMC key
    status = smc_read_key(key, data, &data_len);
    if (EFI_ERROR(status)) {
        return status;
    }

    // Verify we got 2 bytes (fpe2 format)
    if (data_len < 2) {
        return EFI_DEVICE_ERROR;
    }

    // Decode fpe2 to RPM
    *rpm = decode_fpe2(data);

    return EFI_SUCCESS;
}

/**
 * Read fan min/max RPM limits
 */
EFI_STATUS fan_read_min_max(UINT8 fan_index, UINT16 *min_rpm, UINT16 *max_rpm) {
    CHAR8 key[4];
    UINT8 data[32];
    UINT8 data_len = 0;
    EFI_STATUS status;

    if (fan_index >= MAX_FANS || !min_rpm || !max_rpm) {
        return EFI_INVALID_PARAMETER;
    }

    // Read minimum RPM
    build_fan_key(fan_index, KEY_MIN_RPM, key);
    status = smc_read_key(key, data, &data_len);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (data_len < 2) {
        return EFI_DEVICE_ERROR;
    }
    *min_rpm = decode_fpe2(data);

    // Read maximum RPM
    build_fan_key(fan_index, KEY_MAX_RPM, key);
    data_len = 0;
    status = smc_read_key(key, data, &data_len);
    if (EFI_ERROR(status)) {
        return status;
    }
    if (data_len < 2) {
        return EFI_DEVICE_ERROR;
    }
    *max_rpm = decode_fpe2(data);

    return EFI_SUCCESS;
}

/**
 * Check if fan is in manual mode (at SMC level)
 */
EFI_STATUS fan_get_mode(UINT8 fan_index, BOOLEAN *is_manual) {
    CHAR8 key[4];
    UINT8 data[32];
    UINT8 data_len = 0;
    EFI_STATUS status;

    if (fan_index >= MAX_FANS || !is_manual) {
        return EFI_INVALID_PARAMETER;
    }

    // Build key F[n]Md
    build_fan_key(fan_index, KEY_MODE, key);

    // Read SMC key
    status = smc_read_key(key, data, &data_len);
    if (EFI_ERROR(status)) {
        return status;
    }

    if (data_len < 1) {
        return EFI_DEVICE_ERROR;
    }

    // 0 = auto, 1 = manual
    *is_manual = (data[0] != 0);

    return EFI_SUCCESS;
}

/**
 * Calculate target RPM based on temperature and thresholds
 * Uses linear interpolation between min_temp and max_temp
 */
UINT16 fan_calculate_rpm_from_temp(INT16 current_temp, INT16 min_temp, INT16 max_temp,
                                    UINT16 min_rpm, UINT16 max_rpm) {
    // Ensure min < max
    if (min_temp >= max_temp) {
        return min_rpm;  // Safe fallback
    }

    // If below minimum temperature, use minimum RPM
    if (current_temp <= min_temp) {
        return min_rpm;
    }

    // If above maximum temperature, use maximum RPM
    if (current_temp >= max_temp) {
        return max_rpm;
    }

    // Linear interpolation between min and max
    // temp_ratio = (current - min) / (max - min)
    // target_rpm = min_rpm + temp_ratio * (max_rpm - min_rpm)
    INT32 temp_range = max_temp - min_temp;
    INT32 temp_offset = current_temp - min_temp;
    INT32 rpm_range = max_rpm - min_rpm;

    UINT16 target_rpm = min_rpm + (UINT16)((temp_offset * rpm_range) / temp_range);

    // Clamp to safe range (safety check)
    return clamp_rpm(target_rpm, min_rpm, max_rpm);
}

/**
 * Enable/disable sensor-based control for a fan
 */
EFI_STATUS fan_set_sensor_based_mode(UINT8 fan_index, BOOLEAN enable,
                                     UINT8 sensor_index, INT16 min_temp, INT16 max_temp) {
    EFI_STATUS status;

    if (fan_index >= MAX_FANS) {
        return EFI_INVALID_PARAMETER;
    }

    if (enable) {
        // Enable manual mode at SMC level (sensor-based uses manual control)
        status = fan_set_manual_mode(fan_index, TRUE);
        if (EFI_ERROR(status)) {
            return status;
        }
        // Sensor index and temp thresholds are stored in FAN_INFO by caller
    } else {
        // Disable - return to automatic mode
        status = fan_set_manual_mode(fan_index, FALSE);
        if (EFI_ERROR(status)) {
            return status;
        }
    }

    return EFI_SUCCESS;
}

/**
 * Update fan speed based on current temperature
 * Call this periodically for fans in sensor-based mode
 */
EFI_STATUS fan_update_sensor_based(FAN_INFO *fan, INT16 current_temp) {
    UINT16 target_rpm;

    if (!fan) {
        return EFI_INVALID_PARAMETER;
    }

    if (!fan->sensor_based_enabled) {
        return EFI_SUCCESS;  // Not in sensor-based mode
    }

    // Calculate target RPM based on temperature
    target_rpm = fan_calculate_rpm_from_temp(current_temp,
                                             fan->min_temp,
                                             fan->max_temp,
                                             fan->min_rpm,
                                             fan->max_rpm);

    // Set the target RPM
    fan->target_rpm = target_rpm;
    return fan_set_target_rpm(fan->index, target_rpm);
}

/**
 * Set fan to manual or automatic mode
 */
EFI_STATUS fan_set_manual_mode(UINT8 fan_index, BOOLEAN enable) {
    CHAR8 key[4];
    UINT8 data[1];

    if (fan_index >= MAX_FANS) {
        return EFI_INVALID_PARAMETER;
    }

    // Build key F[n]Md
    build_fan_key(fan_index, KEY_MODE, key);

    // Set mode: 0 = auto, 1 = manual
    data[0] = enable ? 1 : 0;

    // Write SMC key
    return smc_write_key(key, data, 1);
}

/**
 * Set target RPM (only effective in manual mode)
 * RPM value is clamped to safe range
 */
EFI_STATUS fan_set_target_rpm(UINT8 fan_index, UINT16 rpm) {
    CHAR8 key[4];
    UINT8 data[2];
    UINT16 min_rpm, max_rpm;
    UINT16 clamped_rpm;
    EFI_STATUS status;

    if (fan_index >= MAX_FANS) {
        return EFI_INVALID_PARAMETER;
    }

    // Read min/max limits for safety
    status = fan_read_min_max(fan_index, &min_rpm, &max_rpm);
    if (EFI_ERROR(status)) {
        return status;
    }

    // Clamp RPM to safe range
    clamped_rpm = clamp_rpm(rpm, min_rpm, max_rpm);

    // Build key F[n]Tg
    build_fan_key(fan_index, KEY_TARGET_RPM, key);

    // Encode RPM to fpe2 format
    encode_fpe2(clamped_rpm, data);

    // Write SMC key
    return smc_write_key(key, data, 2);
}

/**
 * Discover all available fans and populate info array
 */
EFI_STATUS fan_discover_all(FAN_INFO fans[], UINT8 *count) {
    UINT8 i;
    UINT8 fan_count = 0;
    EFI_STATUS status;

    if (!fans || !count) {
        return EFI_INVALID_PARAMETER;
    }

    // Try to detect up to MAX_FANS fans
    for (i = 0; i < MAX_FANS; i++) {
        UINT16 rpm;

        // Try to read current RPM to see if fan exists
        status = fan_read_rpm(i, &rpm);
        if (EFI_ERROR(status)) {
            // Fan doesn't exist or error reading
            continue;
        }

        // Fan exists, populate info
        fans[fan_count].index = i;
        format_fan_label(i, fans[fan_count].label, sizeof(fans[fan_count].label) / sizeof(CHAR16));

        fans[fan_count].current_rpm = rpm;

        // Read min/max
        status = fan_read_min_max(i, &fans[fan_count].min_rpm, &fans[fan_count].max_rpm);
        if (EFI_ERROR(status)) {
            // Use safe defaults if can't read
            fans[fan_count].min_rpm = 600;
            fans[fan_count].max_rpm = 5200;
        }

        // Read SMC mode
        BOOLEAN smc_manual = FALSE;
        status = fan_get_mode(i, &smc_manual);
        if (EFI_ERROR(status)) {
            smc_manual = FALSE;
        }

        // Set mode (default to auto)
        fans[fan_count].mode = smc_manual ? FAN_MODE_MANUAL : FAN_MODE_AUTO;

        // Target RPM (same as current for now)
        fans[fan_count].target_rpm = rpm;

        // Initialize sensor-based settings
        fans[fan_count].sensor_based_enabled = FALSE;
        fans[fan_count].sensor_index = 0;
        fans[fan_count].min_temp = 400;  // 40.0°C
        fans[fan_count].max_temp = 800;  // 80.0°C

        fan_count++;
    }

    *count = fan_count;

    return (fan_count > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/**
 * Restore all fans to automatic mode (safety function)
 */
EFI_STATUS fan_restore_auto_mode_all(void) {
    UINT8 i;
    EFI_STATUS status;
    EFI_STATUS last_error = EFI_SUCCESS;

    // Try to restore all fans to auto mode
    for (i = 0; i < MAX_FANS; i++) {
        status = fan_set_manual_mode(i, FALSE);
        if (EFI_ERROR(status) && status != EFI_NOT_FOUND) {
            // Track last error but continue with other fans
            last_error = status;
        }
    }

    return last_error;
}
