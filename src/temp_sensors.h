#ifndef TEMP_SENSORS_H
#define TEMP_SENSORS_H

// Support both gnu-efi and EDK2/TianoCore build systems
#ifdef _GNU_EFI
  #include <efi.h>
  #include <efilib.h>
#else
  #include <Uefi.h>
  #include <Library/UefiLib.h>
  #include <Library/PrintLib.h>
#endif

// Maximum number of temperature sensors
#define MAX_TEMP_SENSORS 68

// Temperature sensor information structure
typedef struct {
    UINT8 index;              // Sensor index
    CHAR8 key[5];             // SMC key (4 chars + null)
    CHAR16 label[48];         // Human-readable description
    INT16 temperature;        // Temperature in 0.1°C units (e.g., 450 = 45.0°C)
    BOOLEAN valid;            // TRUE if sensor has valid data
} TEMP_SENSOR;

/**
 * Initialization
 */

// Initialize temperature sensor system
EFI_STATUS temp_init(void);

/**
 * Temperature reading functions
 */

// Read temperature from a specific SMC key
EFI_STATUS temp_read_sensor(const CHAR8 key[4], INT16 *temp);

// Discover all available temperature sensors
EFI_STATUS temp_discover_sensors(TEMP_SENSOR sensors[], UINT8 *count);

// Update temperatures for existing sensor list
EFI_STATUS temp_refresh_sensors(TEMP_SENSOR sensors[], UINT8 count);

/**
 * Helper functions
 */

// Get human-readable description for a sensor key
void temp_get_description(const CHAR8 key[4], CHAR16 *description, UINTN desc_size);

// Format temperature for display
void temp_format_display(INT16 temp_decidegrees, CHAR16 *buffer, UINTN buffer_size);

#endif // TEMP_SENSORS_H
