#include "temp_sensors.h"
#include "smc_protocol.h"
#include "utils.h"

// Known temperature sensor keys and their descriptions
// Based on comprehensive Intel/T2 SMC sensor database (2006-2020 Intel Macs, 2018-2019 T2 Macs)
// Sources: macsfancontrol-qt sensordescriptions.cpp, smc_sensor_models_reference.md
// T2 models: Macmini8,1 / MacPro7,1 / MacBookPro15,x+ / MacBookAir8,x+
static const struct {
    const CHAR8 *key;
    const CHAR16 *description;
} sensor_map[] = {
    // Ambient
    {"TA0P", L"Ambient Front"},
    {"TA1P", L"Ambient Rear"},
    {"TA2P", L"Ambient Internal"},
    {"TA3P", L"Ambient Internal 2"},
    {"TA4P", L"Ambient Plenum"},
    {"TA0V", L"Ambient Air"},
    {"TA0S", L"Ambient Sensor"},
    {"TA0D", L"Ambient Diode"},
    {"TA0E", L"Ambient Enclosure"},
    {"TA0T", L"Ambient Top"},
    {"TaLC", L"Ambient Left C"},
    {"TaRC", L"Ambient Right C"},
    {"Tals", L"Ambient Left Side"},
    {"Tars", L"Ambient Right Side"},
    {"Tarl", L"Ambient Rear Left"},

    // CPU Cores (Intel - up to 28 cores for Mac Pro 7,1)
    {"TC0C", L"CPU Core 0"},
    {"TC1C", L"CPU Core 1"},
    {"TC2C", L"CPU Core 2"},
    {"TC3C", L"CPU Core 3"},
    {"TC4C", L"CPU Core 4"},
    {"TC5C", L"CPU Core 5"},
    {"TC6C", L"CPU Core 6"},
    {"TC7C", L"CPU Core 7"},
    {"TC8C", L"CPU Core 8"},
    {"TC9C", L"CPU Core 9"},
    {"TC10C", L"CPU Core 10"},
    {"TC11C", L"CPU Core 11"},
    {"TC12C", L"CPU Core 12"},
    {"TC13C", L"CPU Core 13"},
    {"TC14C", L"CPU Core 14"},
    {"TC15C", L"CPU Core 15"},
    {"TC16C", L"CPU Core 16"},
    {"TC17C", L"CPU Core 17"},
    {"TC18C", L"CPU Core 18"},
    {"TC19C", L"CPU Core 19"},
    {"TC20C", L"CPU Core 20"},
    {"TC21C", L"CPU Core 21"},
    {"TC22C", L"CPU Core 22"},
    {"TC23C", L"CPU Core 23"},
    {"TC24C", L"CPU Core 24"},
    {"TC25C", L"CPU Core 25"},
    {"TC26C", L"CPU Core 26"},
    {"TC27C", L"CPU Core 27"},

    // CPU Thermal
    {"TC0D", L"CPU Diode"},
    {"TC1D", L"CPU Diode 2"},
    {"TC0E", L"CPU Core Average"},
    {"TC0F", L"CPU Core Max"},
    {"TC0H", L"CPU Hot Spot"},
    {"TC0P", L"CPU Proximity"},
    {"TC0G", L"CPU Integrated GPU"},

    // CPU Clusters (Mac Pro dual-socket)
    {"TCAC", L"CPU A Core (PECI)"},
    {"TCAD", L"CPU A Diode"},
    {"TCAG", L"CPU A GPU"},
    {"TCAH", L"CPU A Heatsink"},
    {"TCAS", L"CPU A SRAM"},
    {"TCBC", L"CPU B Core (PECI)"},
    {"TCBD", L"CPU B Diode"},
    {"TCBG", L"CPU B GPU"},
    {"TCBH", L"CPU B Heatsink"},
    {"TCBS", L"CPU B SRAM"},
    {"TCGC", L"CPU GPU PECI"},
    {"TCGc", L"CPU GPU PECI 2"},
    {"TCSC", L"CPU System Cluster"},
    {"TCCD", L"CPU Cross-Domain"},

    // T2 Mac CPU sensors (Macmini8,1 / MacPro7,1 / MacBookPro15+ / MacBookAir8+)
    {"TCSA", L"CPU System Agent"},
    {"TCXC", L"CPU PECI Cross Domain"},
    {"TCaP", L"CPU Package"},
    {"TIED", L"Intel Embedded Device"},

    // GPU (up to 6 cards on Mac Pro 7,1)
    {"TG0D", L"GPU 0 Diode"},
    {"TG1D", L"GPU 1 Diode"},
    {"TG2D", L"GPU 2 Diode"},
    {"TG3D", L"GPU 3 Diode"},
    {"TG4D", L"GPU 4 Diode"},
    {"TG5D", L"GPU 5 Diode"},
    {"TG0P", L"GPU 0 Proximity"},
    {"TG1P", L"GPU 1 Proximity"},
    {"TG2P", L"GPU 2 Proximity"},
    {"TG3P", L"GPU 3 Proximity"},
    {"TG4P", L"GPU 4 Proximity"},
    {"TG5P", L"GPU 5 Proximity"},
    {"TG0C", L"GPU 0 Core"},
    {"TG1C", L"GPU 1 Core"},
    {"TG0S", L"GPU 0 Sensor"},
    {"TG1S", L"GPU 1 Sensor"},
    {"TG0T", L"GPU 0 Die"},
    {"TG1T", L"GPU 1 Die"},
    {"TG2T", L"GPU 2 Die"},
    {"TG3T", L"GPU 3 Die"},
    {"TG4T", L"GPU 4 Die"},
    {"TG5T", L"GPU 5 Die"},
    {"TG0G", L"GPU 0 Graphics"},
    {"TG0H", L"GPU Heatsink"},
    {"TGDD", L"GPU Desktop Discrete"},
    {"TeGG", L"GPU Graphics Thermal Group"},
    {"TeRG", L"GPU RAM Thermal Group"},
    {"TeGP", L"GPU Package"},
    {"TeRP", L"GPU RAM Package"},

    // Memory - Bank Proximity (Mac Pro 8-DIMM support)
    {"Tm0P", L"Memory Bank 0 Proximity"},
    {"Tm1P", L"Memory Bank 1 Proximity"},
    {"Tm2P", L"Memory Bank 2 Proximity"},
    {"Tm3P", L"Memory Bank 3 Proximity"},
    {"Tm4P", L"Memory Bank 4 Proximity"},
    {"Tm5P", L"Memory Bank 5 Proximity"},
    {"Tm6P", L"Memory Bank 6 Proximity"},
    {"Tm7P", L"Memory Bank 7 Proximity"},
    {"TmAS", L"Memory Slot A"},
    {"TmBS", L"Memory Slot B"},
    {"TmCS", L"Memory Slot C"},
    {"TmDS", L"Memory Slot D"},

    // Memory - DIMM Proximity
    {"TM0P", L"Memory Proximity"},
    {"TM1P", L"DIMM Proximity 1"},
    {"TM2P", L"DIMM Proximity 2"},
    {"TM3P", L"DIMM Proximity 3"},
    {"TM4P", L"DIMM Proximity 4"},
    {"TM5P", L"DIMM Proximity 5"},
    {"TM6P", L"DIMM Proximity 6"},
    {"TM7P", L"DIMM Proximity 7"},
    {"TM8P", L"DIMM Proximity 8"},
    {"TM0V", L"Memory Virtual"},
    {"TM0S", L"Memory Slot 0"},
    {"TM1S", L"Memory Slot 1"},
    {"TM2S", L"Memory Slot 2"},
    {"TM3S", L"Memory Slot 3"},
    {"TM4S", L"Memory Slot 4"},
    {"TM5S", L"Memory Slot 5"},
    {"TM6S", L"Memory Slot 6"},
    {"TM7S", L"Memory Slot 7"},
    {"TM8S", L"Memory Slot 8"},
    {"TM9S", L"Memory Slot 9"},
    {"TM10S", L"Memory Slot 10"},
    {"TM11S", L"Memory Slot 11"},

    // Memory - Banks (Mac Pro)
    {"TMA1", L"Memory Bank A1"},
    {"TMA2", L"Memory Bank A2"},
    {"TMA3", L"Memory Bank A3"},
    {"TMA4", L"Memory Bank A4"},
    {"TMB1", L"Memory Bank B1"},
    {"TMB2", L"Memory Bank B2"},
    {"TMB3", L"Memory Bank B3"},
    {"TMB4", L"Memory Bank B4"},
    {"TMHS", L"Memory Heatsink"},
    {"TMLS", L"Memory Low Side"},
    {"TMPS", L"Memory Power Supply"},
    {"TMPV", L"Memory PVDD"},
    {"TMTG", L"Memory Thermal Group"},

    // Storage - Drive Bays (Mac Pro 4,1/5,1/6,1)
    {"TH1P", L"Drive Bay 0"},
    {"TH2P", L"Drive Bay 1"},
    {"TH3P", L"Drive Bay 2"},
    {"TH4P", L"Drive Bay 3"},
    {"HDD0", L"Drive Bay 0 Temp"},
    {"HDD1", L"Drive Bay 1 Temp"},
    {"HDD2", L"Drive Bay 2 Temp"},
    {"HDD3", L"Drive Bay 3 Temp"},
    {"TH1F", L"Drive Bay 1 Front"},
    {"TH1V", L"Drive Bay 1 SATA"},
    {"TH2F", L"Drive Bay 2 Front"},
    {"TH2V", L"Drive Bay 2 SATA"},
    {"TH3F", L"Drive Bay 3 Front"},
    {"TH3V", L"Drive Bay 3 SATA"},
    {"TH4F", L"Drive Bay 4 Front"},
    {"TH4V", L"Drive Bay 4 SATA"},
    {"TH0P", L"HDD Proximity"},
    {"TH0A", L"HDD A"},
    {"TH0B", L"HDD B"},
    {"TH0C", L"HDD C"},
    {"Th0H", L"Drive Thermal"},
    {"Th1H", L"Heatpipe 1"},
    {"Th2H", L"Heatpipe 2"},
    {"THPS", L"HDD Power Supply"},

    // Storage - NVMe via T2 (Macmini8,1 / MacPro7,1 / MacBookPro15+ / MacBookAir8+)
    {"TH0F", L"NVMe Front"},
    {"TH0a", L"NVMe a"},
    {"TH0b", L"NVMe b"},
    {"TS0V", L"SSD Virtual"},

    // PCIe Slots (Mac Pro - 5 slots)
    {"Te1P", L"PCIe Ambient"},
    {"Te1F", L"PCIe Slot 1 Front"},
    {"Te1S", L"PCIe Slot 1 Side"},
    {"Te2F", L"PCIe Slot 2 Front"},
    {"Te2S", L"PCIe Slot 2 Side"},
    {"Te3F", L"PCIe Slot 3 Front"},
    {"Te3S", L"PCIe Slot 3 Side"},
    {"Te4F", L"PCIe Slot 4 Front"},
    {"Te4S", L"PCIe Slot 4 Side"},
    {"Te5F", L"PCIe Slot 5 Front"},
    {"Te5S", L"PCIe Slot 5 Side"},

    // Northbridge / PCH
    {"TN0D", L"Northbridge Diode"},
    {"TN0H", L"Northbridge Heatsink"},
    {"TN0P", L"Northbridge Proximity"},
    {"TN0S", L"Northbridge Sensor"},
    {"TN1P", L"Northbridge 2"},
    {"TNTG", L"Northbridge Thermal Group"},
    {"TPCD", L"PCH Die"},
    {"TPSD", L"PCH SD"},

    // Thunderbolt (T2 Macs)
    {"TTTD", L"Thunderbolt TD"},
    {"TTXD", L"Thunderbolt XD"},

    // Battery (MacBook Pro / Air)
    {"TB0T", L"Battery 0"},
    {"TB1T", L"Battery 1"},
    {"TB2T", L"Battery 2"},
    {"TB3T", L"Battery 3"},
    {"TB0S", L"Battery Sensor 0"},
    {"TB1S", L"Battery Sensor 1"},
    {"TB1F", L"Battery Front"},
    {"TB1M", L"Battery Middle"},
    {"TB1r", L"Battery Rear"},

    // LCD (iMac)
    {"TL0P", L"LCD Proximity"},
    {"TL1P", L"LCD Proximity 2"},

    // Optical Drive
    {"TO0P", L"Optical Drive"},

    // Power Supply
    {"Tp0C", L"Power Supply"},
    {"Tp0P", L"Power Supply Proximity"},
    {"Tp0D", L"Power Supply Diode"},
    {"Tp1C", L"Power Supply 2"},
    {"Tp1P", L"Power Supply Proximity 2"},
    {"TpPS", L"Power Supply Sensor"},
    {"TpTG", L"Power Supply Thermal Group"},
    {"TPMP", L"Power Supply Proximity Alt"},
    {"TV0R", L"Voltage Regulator"},

    // Thermal Groups (Mac Pro)
    {"THTG", L"Thermal Group Target"},

    // Wireless
    {"TW0P", L"Wireless Module"},
    {"TW1P", L"Wireless Module 2"},
    {"TW2P", L"Wireless Module 3"},
    {"TW0S", L"Wireless Sensor"},
    {"TWAP", L"Wireless Alt"},

    // Palm Rest / Trackpad (MacBook Pro)
    {"Ts0P", L"Palm Rest Left"},
    {"Ts1P", L"Palm Rest Right"},
    {"Ts0S", L"Trackpad Sensor 0"},
    {"Ts1S", L"Trackpad Sensor 1"},

    // Enclosure
    {"Te0T", L"Enclosure Top"},
    {"Te1T", L"Enclosure Bottom 1"},
    {"Te2T", L"Enclosure Bottom 2"},
    {"Te3T", L"Enclosure Bottom 3"},
    {"Te4T", L"Enclosure Bottom 4"},
    {"Te5T", L"Enclosure Bottom 5"},

    // Thermal Diodes
    {"TD0P", L"Thermal Diode 0"},
    {"TD1P", L"Thermal Diode 1"},
    {"TD2P", L"Thermal Diode 2"},
    {"TD3P", L"Thermal Diode 3"},

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
