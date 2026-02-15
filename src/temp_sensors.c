#include "temp_sensors.h"
#include "smc_protocol.h"
#include "utils.h"

// Known temperature sensor keys and their descriptions
// Based on macsfancontrol Qt5 application
static const struct {
    const CHAR8 *key;
    const CHAR16 *description;
} sensor_map[] = {
    // Ambient
    {"TA0P", L"Ambient"},

    // CPU A
    {"TCAC", L"CPU A Core (PECI)"},
    {"TCAD", L"CPU A Diode"},
    {"TCAG", L"CPU A GPU"},
    {"TCAH", L"CPU A Heatsink"},
    {"TCAS", L"CPU A SRAM"},
    {"TC0C", L"CPU Core 0"},
    {"TC1C", L"CPU Core 1"},
    {"TC2C", L"CPU Core 2"},
    {"TC3C", L"CPU Core 3"},

    // CPU B
    {"TCBC", L"CPU B Core (PECI)"},
    {"TCBD", L"CPU B Diode"},
    {"TCBG", L"CPU B GPU"},
    {"TCBH", L"CPU B Heatsink"},
    {"TCBS", L"CPU B SRAM"},
    {"TC4C", L"CPU Core 4"},
    {"TC5C", L"CPU Core 5"},
    {"TC6C", L"CPU Core 6"},
    {"TC7C", L"CPU Core 7"},

    // Drive Bays
    {"TH1P", L"Drive Bay 0"},
    {"TH2P", L"Drive Bay 1"},
    {"TH3P", L"Drive Bay 2"},
    {"TH4P", L"Drive Bay 3"},

    // Memory (DIMM Proximity)
    {"TM1P", L"DIMM Proximity 1"},
    {"TM2P", L"DIMM Proximity 2"},
    {"TM3P", L"DIMM Proximity 3"},
    {"TM4P", L"DIMM Proximity 4"},
    {"TM5P", L"DIMM Proximity 5"},
    {"TM6P", L"DIMM Proximity 6"},
    {"TM7P", L"DIMM Proximity 7"},
    {"TM8P", L"DIMM Proximity 8"},

    // IOH (Northbridge)
    {"TN0D", L"IOH Diode"},
    {"TN0H", L"IOH Heatsink"},

    // PCIe/Enclosure
    {"Te1P", L"PCIe Ambient"},

    // Power Supply
    {"Tp0C", L"AC/DC Supply 1"},
    {"Tp1C", L"AC/DC Supply 2"},

    // GPU
    {"TG0D", L"GPU Diode"},
    {"TG0P", L"GPU Proximity"},

    {NULL, NULL}  // Terminator
};

/**
 * Initialize temperature sensor system
 */
EFI_STATUS temp_init(void) {
    // Nothing specific to initialize
    return EFI_SUCCESS;
}

/**
 * Get human-readable description for a sensor key
 */
void temp_get_description(const CHAR8 key[4], CHAR16 *description, UINTN desc_size) {
    UINTN i;

    if (!key || !description || desc_size == 0) {
        return;
    }

    // Search for matching key in sensor map
    for (i = 0; sensor_map[i].key != NULL; i++) {
        if (key[0] == sensor_map[i].key[0] &&
            key[1] == sensor_map[i].key[1] &&
            key[2] == sensor_map[i].key[2] &&
            key[3] == sensor_map[i].key[3]) {
            // Found match - copy description
            UINTN j;
            const CHAR16 *src = sensor_map[i].description;
            for (j = 0; j < desc_size - 1 && src[j] != L'\0'; j++) {
                description[j] = src[j];
            }
            description[j] = L'\0';
            return;
        }
    }

    // No match found - just copy the key
    ascii_to_wide(key, description, desc_size);
}

/**
 * Read temperature from a specific SMC key
 * Temperature is returned in decidegrees Celsius (0.1°C units)
 * For example: 450 = 45.0°C
 */
EFI_STATUS temp_read_sensor(const CHAR8 key[4], INT16 *temp) {
    UINT8 data[32];
    UINT8 data_len = 0;
    EFI_STATUS status;

    if (!key || !temp) {
        return EFI_INVALID_PARAMETER;
    }

    // Read SMC key
    status = smc_read_key(key, data, &data_len);
    if (EFI_ERROR(status)) {
        return status;
    }

    // Verify we got 2 bytes (sp78 format)
    if (data_len < 2) {
        return EFI_DEVICE_ERROR;
    }

    // Decode sp78 format (signed fixed-point, 7.8 bits)
    // Formula: temp_celsius = value / 256.0
    // We want decidegrees, so: temp_decidegrees = (value * 10) / 256
    INT16 value = (INT16)((data[0] << 8) | data[1]);

    // Convert to decidegrees Celsius
    // value / 256.0 * 10 = value * 10 / 256
    *temp = (value * 10) / 256;

    return EFI_SUCCESS;
}

/**
 * Discover all available temperature sensors
 * Tries common sensor keys and returns those that respond
 */
EFI_STATUS temp_discover_sensors(TEMP_SENSOR sensors[], UINT8 *count) {
    UINT8 sensor_count = 0;
    UINTN i;

    if (!sensors || !count) {
        return EFI_INVALID_PARAMETER;
    }

    // Try all known sensor keys
    for (i = 0; sensor_map[i].key != NULL && sensor_count < MAX_TEMP_SENSORS; i++) {
        INT16 temp;
        EFI_STATUS status;

        // Try to read this sensor
        status = temp_read_sensor((const CHAR8 *)sensor_map[i].key, &temp);
        if (!EFI_ERROR(status)) {
            // Sensor exists and returned valid data
            // Check if temperature is reasonable (not -128°C which indicates error)
            if (temp > -1000) {  // -100°C in decidegrees
                sensors[sensor_count].index = sensor_count;

                // Copy key
                UINTN j;
                for (j = 0; j < 4; j++) {
                    sensors[sensor_count].key[j] = sensor_map[i].key[j];
                }
                sensors[sensor_count].key[4] = '\0';

                // Copy description
                UINTN desc_len = 0;
                const CHAR16 *desc = sensor_map[i].description;
                while (desc[desc_len] != L'\0' && desc_len < 47) {
                    sensors[sensor_count].label[desc_len] = desc[desc_len];
                    desc_len++;
                }
                sensors[sensor_count].label[desc_len] = L'\0';

                sensors[sensor_count].temperature = temp;
                sensors[sensor_count].valid = TRUE;

                sensor_count++;
            }
        }
    }

    *count = sensor_count;

    return (sensor_count > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/**
 * Update temperatures for existing sensor list
 * Faster than rediscovering - just reads known sensors
 */
EFI_STATUS temp_refresh_sensors(TEMP_SENSOR sensors[], UINT8 count) {
    UINT8 i;

    if (!sensors) {
        return EFI_INVALID_PARAMETER;
    }

    for (i = 0; i < count; i++) {
        INT16 temp;
        EFI_STATUS status;

        status = temp_read_sensor(sensors[i].key, &temp);
        if (!EFI_ERROR(status)) {
            sensors[i].temperature = temp;
            sensors[i].valid = TRUE;
        } else {
            sensors[i].valid = FALSE;
        }
    }

    return EFI_SUCCESS;
}

/**
 * Format temperature for display
 * Converts decidegrees to readable string (e.g., "45.5°C")
 */
void temp_format_display(INT16 temp_decidegrees, CHAR16 *buffer, UINTN buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }

    // Convert decidegrees to whole degrees and tenths
    INT16 degrees = temp_decidegrees / 10;
    INT16 tenths = temp_decidegrees % 10;
    if (tenths < 0) tenths = -tenths;  // Handle negative temperatures

    UnicodeSPrint(buffer, buffer_size, L"%d.%d°C", degrees, tenths);
}
