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
 * Check if fan is in manual mode
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

        // Read mode
        status = fan_get_mode(i, &fans[fan_count].is_manual);
        if (EFI_ERROR(status)) {
            fans[fan_count].is_manual = FALSE;
        }

        // Target RPM (same as current for now)
        fans[fan_count].target_rpm = rpm;

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
