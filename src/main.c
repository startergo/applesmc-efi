#include <efi.h>
#include <efilib.h>

#include "smc_protocol.h"
#include "fan_control.h"
#include "ui_menu.h"
#include "utils.h"

/**
 * UEFI Application Entry Point
 * This is the main function that will be called when the EFI application starts
 */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    FAN_INFO fans[MAX_FANS];
    UINT8 fan_count = 0;

    // Initialize gnu-efi library
    InitializeLib(ImageHandle, SystemTable);

    // Clear screen and display banner
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"Apple SMC Fan Control v1.0 (UEFI)\n");
    Print(L"===================================\n\n");

    // Detect SMC hardware
    Print(L"Detecting Apple SMC...\n");
    if (!smc_detect()) {
        Print(L"\nERROR: Apple SMC not detected\n");
        Print(L"This application requires Apple hardware with SMC.\n\n");
        Print(L"Press any key to exit.\n");

        UINTN Index;
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        return EFI_UNSUPPORTED;
    }
    Print(L"SMC detected successfully!\n\n");

    // Initialize fan control
    Print(L"Initializing fan control...\n");
    status = fan_init();
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to initialize fan control (Status: 0x%x)\n", status);
        Print(L"\nPress any key to exit.\n");

        UINTN Index;
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        return EFI_DEVICE_ERROR;
    }

    // Discover fans
    Print(L"Discovering fans...\n");
    status = fan_discover_all(fans, &fan_count);
    if (EFI_ERROR(status) || fan_count == 0) {
        Print(L"ERROR: No fans detected\n");
        Print(L"\nPress any key to exit.\n");

        UINTN Index;
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        return EFI_NOT_FOUND;
    }

    Print(L"Found %d fans!\n\n", fan_count);

    // Display detected fans
    Print(L"Detected fans:\n");
    for (UINT8 i = 0; i < fan_count; i++) {
        Print(L"  [%d] %s - %d RPM (range: %d-%d)\n",
              i,
              fans[i].label,
              fans[i].current_rpm,
              fans[i].min_rpm,
              fans[i].max_rpm);
    }
    Print(L"\n");

    // Wait before starting interactive menu
    Print(L"Press any key to start interactive fan control...\n");
    {
        UINTN Index;
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    }

    // Run interactive menu
    ui_menu_run(fans, fan_count);

    // Safety: Restore all fans to automatic mode before exit
    Print(L"\n");
    Print(L"Restoring all fans to automatic mode...\n");
    status = fan_restore_auto_mode_all();
    if (EFI_ERROR(status)) {
        Print(L"Warning: Some fans may not have been restored to auto mode\n");
    } else {
        Print(L"All fans restored to automatic mode.\n");
    }

    Print(L"\n");
    Print(L"Thank you for using Apple SMC Fan Control!\n");
    Print(L"Press any key to exit.\n");

    {
        UINTN Index;
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    }

    return EFI_SUCCESS;
}
