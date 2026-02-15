#include "ui_menu.h"
#include "fan_control.h"
#include "temp_sensors.h"
#include "utils.h"

#define RPM_STEP 100     // RPM increment/decrement step
#define TEMP_STEP 50     // Temperature threshold increment (5.0°C in decidegrees)

/**
 * Clear the screen using UEFI ConOut
 */
void ui_clear_screen(void) {
    gST->ConOut->ClearScreen(gST->ConOut);
}

/**
 * Display header banner
 */
void ui_display_header(void) {
    Print(L"========================================\n");
    Print(L"    Apple SMC Fan Control (UEFI)\n");
    Print(L"========================================\n\n");
}

/**
 * Display fan information table
 */
void ui_display_fans(FAN_INFO fans[], UINT8 count, INT8 selected_fan) {
    UINT8 i;

    Print(L"Detected Fans:\n");
    for (i = 0; i < count; i++) {
        // Selection indicator
        if (i == selected_fan) {
            Print(L">");
        } else {
            Print(L" ");
        }

        // Fan index and label
        Print(L"[%d] %-10s: %4d RPM (%4d-%4d)  ",
              i,
              fans[i].label,
              fans[i].current_rpm,
              fans[i].min_rpm,
              fans[i].max_rpm);

        // Mode display
        switch (fans[i].mode) {
            case FAN_MODE_AUTO:
                Print(L"[AUTO]\n");
                break;
            case FAN_MODE_MANUAL:
                Print(L"[MANUAL: %d]\n", fans[i].target_rpm);
                break;
            case FAN_MODE_SENSOR_BASED:
                if (fans[i].sensor_based_enabled) {
                    Print(L"[SENSOR: %d RPM, %d.%d-%d.%d°C]\n",
                          fans[i].target_rpm,
                          fans[i].min_temp / 10, fans[i].min_temp % 10,
                          fans[i].max_temp / 10, fans[i].max_temp % 10);
                } else {
                    Print(L"[SENSOR: not configured]\n");
                }
                break;
        }
    }
    Print(L"\n");
}

/**
 * Display command help
 */
void ui_display_help(void) {
    Print(L"Commands:\n");
    Print(L"  [0-5]  Select fan\n");
    Print(L"  [a]    Set to Auto mode\n");
    Print(L"  [m]    Set to Manual mode\n");
    Print(L"  [s]    Set to Sensor-based mode\n");
    Print(L"  [+]    Increase RPM/Temp (+%d RPM or +%.1f°C)\n", RPM_STEP, TEMP_STEP / 10.0);
    Print(L"  [-]    Decrease RPM/Temp (-%d RPM or -%.1f°C)\n", RPM_STEP, TEMP_STEP / 10.0);
    Print(L"  [<]    Lower min temp threshold\n");
    Print(L"  [>]    Raise max temp threshold\n");
    Print(L"  [t]    View temperature sensors\n");
    Print(L"  [r]    Refresh display\n");
    Print(L"  [q]    Quit\n");
    Print(L"\nSelect: ");
}

/**
 * Display status message
 */
void ui_display_status(const CHAR16 *message) {
    Print(L"\n[Status: %s]\n", message);
}

/**
 * Refresh fan data and update sensor-based fans
 */
static void refresh_fan_data(FAN_INFO fans[], UINT8 count, TEMP_SENSOR sensors[], UINT8 sensor_count) {
    UINT8 i;

    // Refresh temperature sensors
    if (sensor_count > 0) {
        temp_refresh_sensors(sensors, sensor_count);
    }

    for (i = 0; i < count; i++) {
        // Update current RPM
        fan_read_rpm(fans[i].index, &fans[i].current_rpm);

        // Update sensor-based fans
        if (fans[i].mode == FAN_MODE_SENSOR_BASED && fans[i].sensor_based_enabled) {
            if (fans[i].sensor_index < sensor_count) {
                INT16 current_temp = sensors[fans[i].sensor_index].temperature;
                fan_update_sensor_based(&fans[i], current_temp);
            }
        }
    }
}

/**
 * Display temperature sensors
 */
static void display_temp_sensors(TEMP_SENSOR sensors[], UINT8 count, INT8 selected_sensor) {
    UINT8 i;
    CHAR16 temp_str[16];

    ui_clear_screen();
    Print(L"========================================\n");
    Print(L"       Temperature Sensors\n");
    Print(L"========================================\n\n");

    for (i = 0; i < count && i < 20; i++) {  // Show first 20
        if (i == selected_sensor) {
            Print(L">");
        } else {
            Print(L" ");
        }

        temp_format_display(sensors[i].temperature, temp_str, sizeof(temp_str));
        Print(L"[%2d] %-4s - %-30s: %s\n",
              i,
              sensors[i].key,
              sensors[i].label,
              temp_str);
    }

    Print(L"\nPress any key to return...\n");
}

/**
 * Main interactive menu loop
 */
void ui_menu_run(FAN_INFO fans[], UINT8 count) {
    INT8 selected_fan = -1;  // -1 means no fan selected
    BOOLEAN running = TRUE;
    EFI_INPUT_KEY key;
    EFI_STATUS status;
    CHAR16 status_msg[128];

    // Discover temperature sensors
    TEMP_SENSOR sensors[MAX_TEMP_SENSORS];
    UINT8 sensor_count = 0;

    Print(L"Discovering temperature sensors...\n");
    status = temp_discover_sensors(sensors, &sensor_count);
    if (EFI_ERROR(status) || sensor_count == 0) {
        Print(L"Warning: No temperature sensors found\n");
        sensor_count = 0;
    } else {
        Print(L"Found %d temperature sensors\n", sensor_count);
    }

    delay_milliseconds(1000);

    UnicodeSPrint(status_msg, sizeof(status_msg), L"Ready - %d sensors available", sensor_count);

    while (running) {
        // Refresh fan data (including sensor-based updates)
        refresh_fan_data(fans, count, sensors, sensor_count);

        // Clear and redraw screen
        ui_clear_screen();
        ui_display_header();

        // Display fans
        ui_display_fans(fans, count, selected_fan);

        // Display status
        ui_display_status(status_msg);
        Print(L"\n");

        // Display help
        ui_display_help();

        // Wait for key press
        UINTN index;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);

        // Read key
        status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        if (EFI_ERROR(status)) {
            continue;
        }

        // Handle key input
        CHAR16 ch = key.UnicodeChar;

        // Fan selection (0-5)
        if (ch >= L'0' && ch <= L'5') {
            UINT8 fan_index = (UINT8)(ch - L'0');
            if (fan_index < count) {
                selected_fan = fan_index;
                UnicodeSPrint(status_msg, sizeof(status_msg), L"Selected fan %d (%s)",
                             fan_index, fans[fan_index].label);
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"Invalid fan index");
            }
        }
        // Auto mode
        else if (ch == L'a' || ch == L'A') {
            if (selected_fan >= 0 && selected_fan < count) {
                fans[selected_fan].sensor_based_enabled = FALSE;
                status = fan_set_manual_mode(fans[selected_fan].index, FALSE);
                if (!EFI_ERROR(status)) {
                    fans[selected_fan].mode = FAN_MODE_AUTO;
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan %d set to AUTO mode",
                                 selected_fan);
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Failed to set AUTO mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Manual mode
        else if (ch == L'm' || ch == L'M') {
            if (selected_fan >= 0 && selected_fan < count) {
                fans[selected_fan].sensor_based_enabled = FALSE;
                status = fan_set_manual_mode(fans[selected_fan].index, TRUE);
                if (!EFI_ERROR(status)) {
                    fans[selected_fan].mode = FAN_MODE_MANUAL;
                    // Set initial target to current RPM
                    fans[selected_fan].target_rpm = fans[selected_fan].current_rpm;
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan %d set to MANUAL mode",
                                 selected_fan);
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Failed to set MANUAL mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Sensor-based mode
        else if (ch == L's' || ch == L'S') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (sensor_count == 0) {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"No sensors available");
                } else {
                    // Enable sensor-based mode with first sensor
                    fans[selected_fan].mode = FAN_MODE_SENSOR_BASED;
                    fans[selected_fan].sensor_based_enabled = TRUE;
                    fans[selected_fan].sensor_index = 0;
                    fans[selected_fan].min_temp = 400;  // 40.0°C
                    fans[selected_fan].max_temp = 800;  // 80.0°C

                    status = fan_set_sensor_based_mode(fans[selected_fan].index, TRUE,
                                                       0, 400, 800);
                    if (!EFI_ERROR(status)) {
                        UnicodeSPrint(status_msg, sizeof(status_msg),
                                     L"Fan %d: SENSOR mode (use +/- to select sensor)",
                                     selected_fan);
                    } else {
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Failed to set SENSOR mode");
                    }
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Increase RPM or cycle sensor
        else if (ch == L'+' || ch == L'=') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].mode == FAN_MODE_MANUAL) {
                    UINT16 new_rpm = fans[selected_fan].target_rpm + RPM_STEP;
                    new_rpm = clamp_rpm(new_rpm, fans[selected_fan].min_rpm,
                                       fans[selected_fan].max_rpm);

                    status = fan_set_target_rpm(fans[selected_fan].index, new_rpm);
                    if (!EFI_ERROR(status)) {
                        fans[selected_fan].target_rpm = new_rpm;
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Target RPM: %d", new_rpm);
                    } else {
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Failed to set RPM");
                    }
                } else if (fans[selected_fan].mode == FAN_MODE_SENSOR_BASED) {
                    // Cycle to next sensor
                    if (sensor_count > 0) {
                        fans[selected_fan].sensor_index = (fans[selected_fan].sensor_index + 1) % sensor_count;
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Sensor: %s (%s)",
                                     sensors[fans[selected_fan].sensor_index].key,
                                     sensors[fans[selected_fan].sensor_index].label);
                    }
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in MANUAL or SENSOR mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Decrease RPM or cycle sensor
        else if (ch == L'-' || ch == L'_') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].mode == FAN_MODE_MANUAL) {
                    INT32 new_rpm = (INT32)fans[selected_fan].target_rpm - RPM_STEP;
                    if (new_rpm < 0) new_rpm = 0;

                    new_rpm = clamp_rpm((UINT16)new_rpm, fans[selected_fan].min_rpm,
                                       fans[selected_fan].max_rpm);

                    status = fan_set_target_rpm(fans[selected_fan].index, (UINT16)new_rpm);
                    if (!EFI_ERROR(status)) {
                        fans[selected_fan].target_rpm = (UINT16)new_rpm;
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Target RPM: %d", new_rpm);
                    } else {
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Failed to set RPM");
                    }
                } else if (fans[selected_fan].mode == FAN_MODE_SENSOR_BASED) {
                    // Cycle to previous sensor
                    if (sensor_count > 0) {
                        if (fans[selected_fan].sensor_index == 0) {
                            fans[selected_fan].sensor_index = sensor_count - 1;
                        } else {
                            fans[selected_fan].sensor_index--;
                        }
                        UnicodeSPrint(status_msg, sizeof(status_msg), L"Sensor: %s (%s)",
                                     sensors[fans[selected_fan].sensor_index].key,
                                     sensors[fans[selected_fan].sensor_index].label);
                    }
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in MANUAL or SENSOR mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Lower min temp threshold
        else if (ch == L'<' || ch == L',') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].mode == FAN_MODE_SENSOR_BASED) {
                    fans[selected_fan].min_temp -= TEMP_STEP;
                    if (fans[selected_fan].min_temp < 0) fans[selected_fan].min_temp = 0;
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Min temp: %d.%d°C",
                                 fans[selected_fan].min_temp / 10,
                                 fans[selected_fan].min_temp % 10);
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in SENSOR mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Raise max temp threshold
        else if (ch == L'>' || ch == L'.') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].mode == FAN_MODE_SENSOR_BASED) {
                    fans[selected_fan].max_temp += TEMP_STEP;
                    if (fans[selected_fan].max_temp > 1200) fans[selected_fan].max_temp = 1200;  // 120°C max
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Max temp: %d.%d°C",
                                 fans[selected_fan].max_temp / 10,
                                 fans[selected_fan].max_temp % 10);
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in SENSOR mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // View temperature sensors
        else if (ch == L't' || ch == L'T') {
            if (sensor_count > 0) {
                // Refresh sensors
                temp_refresh_sensors(sensors, sensor_count);
                display_temp_sensors(sensors, sensor_count, -1);

                // Wait for key press
                UINTN idx;
                EFI_INPUT_KEY k;
                gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
                gST->ConIn->ReadKeyStroke(gST->ConIn, &k);
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No sensors available");
            }
        }
        // Refresh
        else if (ch == L'r' || ch == L'R') {
            refresh_fan_data(fans, count, sensors, sensor_count);
            UnicodeSPrint(status_msg, sizeof(status_msg), L"Refreshed");
        }
        // Quit
        else if (ch == L'q' || ch == L'Q') {
            running = FALSE;
            UnicodeSPrint(status_msg, sizeof(status_msg), L"Exiting...");
        }
    }
}
