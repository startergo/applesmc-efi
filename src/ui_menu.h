#ifndef UI_MENU_H
#define UI_MENU_H

#include <efi.h>
#include <efilib.h>
#include "fan_control.h"

/**
 * UI display functions
 */

// Clear the screen
void ui_clear_screen(void);

// Display header banner
void ui_display_header(void);

// Display fan information table
void ui_display_fans(FAN_INFO fans[], UINT8 count, INT8 selected_fan);

// Display command help
void ui_display_help(void);

// Display status message
void ui_display_status(const CHAR16 *message);

/**
 * Main menu loop
 */

// Run interactive menu
void ui_menu_run(FAN_INFO fans[], UINT8 count);

#endif // UI_MENU_H
