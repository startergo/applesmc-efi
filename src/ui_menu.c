#include "ui_menu.h"
#include "fan_control.h"
#include "utils.h"

#define RPM_STEP 100  // RPM increment/decrement step

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
        if (fans[i].is_manual) {
            Print(L"[MANUAL: %d]\n", fans[i].target_rpm);
        } else {
            Print(L"[AUTO]\n");
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
    Print(L"  [+]    Increase RPM (+%d)\n", RPM_STEP);
    Print(L"  [-]    Decrease RPM (-%d)\n", RPM_STEP);
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
 * Refresh fan data
 */
static void refresh_fan_data(FAN_INFO fans[], UINT8 count) {
    UINT8 i;

    for (i = 0; i < count; i++) {
        // Update current RPM
        fan_read_rpm(fans[i].index, &fans[i].current_rpm);

        // Update mode
        fan_get_mode(fans[i].index, &fans[i].is_manual);
    }
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

    UnicodeSPrint(status_msg, sizeof(status_msg), L"Ready");

    while (running) {
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
                status = fan_set_manual_mode(fans[selected_fan].index, FALSE);
                if (!EFI_ERROR(status)) {
                    fans[selected_fan].is_manual = FALSE;
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
                status = fan_set_manual_mode(fans[selected_fan].index, TRUE);
                if (!EFI_ERROR(status)) {
                    fans[selected_fan].is_manual = TRUE;
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
        // Increase RPM
        else if (ch == L'+' || ch == L'=') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].is_manual) {
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
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in MANUAL mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Decrease RPM
        else if (ch == L'-' || ch == L'_') {
            if (selected_fan >= 0 && selected_fan < count) {
                if (fans[selected_fan].is_manual) {
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
                } else {
                    UnicodeSPrint(status_msg, sizeof(status_msg), L"Fan must be in MANUAL mode");
                }
            } else {
                UnicodeSPrint(status_msg, sizeof(status_msg), L"No fan selected");
            }
        }
        // Refresh
        else if (ch == L'r' || ch == L'R') {
            refresh_fan_data(fans, count);
            UnicodeSPrint(status_msg, sizeof(status_msg), L"Refreshed");
        }
        // Quit
        else if (ch == L'q' || ch == L'Q') {
            running = FALSE;
            UnicodeSPrint(status_msg, sizeof(status_msg), L"Exiting...");
        }
    }
}
