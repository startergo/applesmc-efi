#include "smc_protocol.h"

// Global variable to store last error
static UINT8 last_error = 0;

/**
 * Direct I/O port access using inline assembly
 * x86_64 specific implementation
 */

UINT8 smc_inb(UINT16 port) {
    UINT8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void smc_outb(UINT16 port, UINT8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Microsecond delay using UEFI Boot Services
 */
static void smc_delay_us(UINT32 microseconds) {
    gBS->Stall(microseconds);
}

/**
 * Wait for specific status with timeout
 * Returns EFI_SUCCESS if status achieved, EFI_TIMEOUT otherwise
 */
EFI_STATUS smc_wait_status(UINT8 expected_status, UINT32 timeout_us) {
    UINT32 elapsed = 0;
    UINT8 status;

    while (elapsed < timeout_us) {
        status = smc_inb(APPLESMC_CMD_PORT);

        // Check if we have the expected status
        if ((status & expected_status) == expected_status) {
            return EFI_SUCCESS;
        }

        // Small delay before next check
        smc_delay_us(SMC_IO_DELAY_US);
        elapsed += SMC_IO_DELAY_US;
    }

    return EFI_TIMEOUT;
}

/**
 * Get last SMC error code from error port
 */
UINT8 smc_get_last_error(void) {
    last_error = smc_inb(APPLESMC_ERR_PORT);
    return last_error;
}

/**
 * Clear SMC error status
 */
void smc_clear_error(void) {
    // Write to CMD port to clear error state
    smc_outb(APPLESMC_CMD_PORT, APPLESMC_ST_CMD_DONE);
    smc_delay_us(SMC_IO_DELAY_US);
    last_error = 0;
}

/**
 * Initialize SMC interface
 */
EFI_STATUS smc_init(void) {
    // Clear any pending errors
    smc_clear_error();

    // Verify CMD port is accessible
    UINT8 status = smc_inb(APPLESMC_CMD_PORT);

    // Status should be reasonable (not 0xFF which indicates no hardware)
    if (status == 0xFF) {
        return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

/**
 * Detect if SMC is present by trying to read a known key
 * We'll try reading "REV " (revision) which should exist on all Macs
 */
BOOLEAN smc_detect(void) {
    UINT8 data[32];
    UINT8 data_len = 0;
    CHAR8 test_key[4] = {'R', 'E', 'V', ' '};

    EFI_STATUS status = smc_read_key(test_key, data, &data_len);

    return (status == EFI_SUCCESS && data_len > 0);
}

/**
 * Read SMC key value
 * Implements the READ command state machine from QEMU applesmc.c
 *
 * Protocol sequence:
 * 1. Write READ_CMD to CMD port
 * 2. Wait for ACK status
 * 3. Write 4-byte key to DATA port (one byte at a time)
 * 4. Wait for DATA_READY after 4th byte
 * 5. Read data length
 * 6. Read data bytes from DATA port
 * 7. Status returns to CMD_DONE
 */
EFI_STATUS smc_read_key(const CHAR8 key[4], UINT8 *data, UINT8 *data_len) {
    EFI_STATUS status;
    UINT8 i;

    if (!key || !data || !data_len) {
        return EFI_INVALID_PARAMETER;
    }

    // Step 1: Write READ command
    smc_outb(APPLESMC_CMD_PORT, APPLESMC_READ_CMD);
    smc_delay_us(SMC_IO_DELAY_US);

    // Step 2: Wait for ACK
    status = smc_wait_status(APPLESMC_ST_ACK, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        smc_get_last_error();
        return EFI_DEVICE_ERROR;
    }

    // Step 3: Write 4-byte key
    for (i = 0; i < 4; i++) {
        smc_outb(APPLESMC_DATA_PORT, key[i]);
        smc_delay_us(SMC_IO_DELAY_US);

        // After 4th byte, wait for DATA_READY
        if (i == 3) {
            status = smc_wait_status(APPLESMC_ST_DATA_READY, SMC_STATUS_TIMEOUT_US);
            if (EFI_ERROR(status)) {
                UINT8 error = smc_get_last_error();
                if (error == APPLESMC_ST_1E_NOEXIST) {
                    return EFI_NOT_FOUND;
                }
                return EFI_DEVICE_ERROR;
            }
        }
    }

    // Step 5: Read data length
    *data_len = smc_inb(APPLESMC_DATA_PORT);
    smc_delay_us(SMC_IO_DELAY_US);

    // Sanity check on data length
    if (*data_len > SMC_MAX_DATA_LENGTH) {
        *data_len = SMC_MAX_DATA_LENGTH;
    }

    // Step 6: Read data bytes
    for (i = 0; i < *data_len; i++) {
        data[i] = smc_inb(APPLESMC_DATA_PORT);
        smc_delay_us(SMC_IO_DELAY_US);
    }

    // Wait for command completion
    status = smc_wait_status(APPLESMC_ST_CMD_DONE, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

/**
 * Write SMC key value
 * Implements the WRITE command state machine
 *
 * Protocol sequence:
 * 1. Write WRITE_CMD to CMD port
 * 2. Wait for ACK status
 * 3. Write 4-byte key to DATA port
 * 4. Write data length
 * 5. Write data bytes to DATA port
 * 6. Wait for CMD_DONE
 */
EFI_STATUS smc_write_key(const CHAR8 key[4], const UINT8 *data, UINT8 data_len) {
    EFI_STATUS status;
    UINT8 i;

    if (!key || !data || data_len == 0 || data_len > SMC_MAX_DATA_LENGTH) {
        return EFI_INVALID_PARAMETER;
    }

    // Step 1: Write WRITE command
    smc_outb(APPLESMC_CMD_PORT, APPLESMC_WRITE_CMD);
    smc_delay_us(SMC_IO_DELAY_US);

    // Step 2: Wait for ACK
    status = smc_wait_status(APPLESMC_ST_ACK, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        smc_get_last_error();
        return EFI_DEVICE_ERROR;
    }

    // Step 3: Write 4-byte key
    for (i = 0; i < 4; i++) {
        smc_outb(APPLESMC_DATA_PORT, key[i]);
        smc_delay_us(SMC_IO_DELAY_US);
    }

    // Step 4: Write data length
    smc_outb(APPLESMC_DATA_PORT, data_len);
    smc_delay_us(SMC_IO_DELAY_US);

    // Step 5: Write data bytes
    for (i = 0; i < data_len; i++) {
        smc_outb(APPLESMC_DATA_PORT, data[i]);
        smc_delay_us(SMC_IO_DELAY_US);
    }

    // Step 6: Wait for command completion
    status = smc_wait_status(APPLESMC_ST_CMD_DONE, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        UINT8 error = smc_get_last_error();
        if (error == APPLESMC_ST_1E_READONLY) {
            return EFI_WRITE_PROTECTED;
        }
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

/**
 * Get key type information
 * Returns the data size and type code for a given key
 */
EFI_STATUS smc_get_key_type(const CHAR8 key[4], UINT8 *data_size, CHAR8 type[5]) {
    EFI_STATUS status;
    UINT8 i;

    if (!key || !data_size || !type) {
        return EFI_INVALID_PARAMETER;
    }

    // Write GET_KEY_TYPE command
    smc_outb(APPLESMC_CMD_PORT, APPLESMC_GET_KEY_TYPE_CMD);
    smc_delay_us(SMC_IO_DELAY_US);

    // Wait for ACK
    status = smc_wait_status(APPLESMC_ST_ACK, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        return EFI_DEVICE_ERROR;
    }

    // Write 4-byte key
    for (i = 0; i < 4; i++) {
        smc_outb(APPLESMC_DATA_PORT, key[i]);
        smc_delay_us(SMC_IO_DELAY_US);
    }

    // Wait for DATA_READY
    status = smc_wait_status(APPLESMC_ST_DATA_READY, SMC_STATUS_TIMEOUT_US);
    if (EFI_ERROR(status)) {
        return EFI_DEVICE_ERROR;
    }

    // Read 6 bytes (1 byte length + 4 bytes type + 1 byte attributes)
    *data_size = smc_inb(APPLESMC_DATA_PORT);
    smc_delay_us(SMC_IO_DELAY_US);

    for (i = 0; i < 4; i++) {
        type[i] = smc_inb(APPLESMC_DATA_PORT);
        smc_delay_us(SMC_IO_DELAY_US);
    }
    type[4] = '\0';  // Null terminate

    // Read and discard attributes byte
    smc_inb(APPLESMC_DATA_PORT);

    return EFI_SUCCESS;
}
