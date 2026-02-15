#ifndef SMC_PROTOCOL_H
#define SMC_PROTOCOL_H

// Support both gnu-efi and EDK2/TianoCore build systems
#ifdef _GNU_EFI
  #include <efi.h>
  #include <efilib.h>
#else
  #include <Uefi.h>
  #include <Library/UefiLib.h>
  #include <Library/UefiBootServicesTableLib.h>
#endif

// SMC I/O Port Addresses
#define APPLESMC_DATA_PORT      0x300  // Data read/write port
#define APPLESMC_CMD_PORT       0x304  // Command and status port
#define APPLESMC_ERR_PORT       0x31E  // Error status port

// SMC Commands
#define APPLESMC_READ_CMD               0x10  // Read SMC key value
#define APPLESMC_WRITE_CMD              0x11  // Write SMC key value
#define APPLESMC_GET_KEY_BY_INDEX_CMD   0x12  // Enumerate keys by index
#define APPLESMC_GET_KEY_TYPE_CMD       0x13  // Get key data type

// SMC Status Flags (read from CMD port)
#define APPLESMC_ST_CMD_DONE    0x00  // Operation complete
#define APPLESMC_ST_DATA_READY  0x01  // Data available for reading
#define APPLESMC_ST_BUSY        0x02  // Device busy
#define APPLESMC_ST_ACK         0x04  // Command acknowledged
#define APPLESMC_ST_NEW_CMD     0x08  // New command received

// SMC Error Codes (read from ERR port)
#define APPLESMC_ST_1E_CMD_INTRUPTED    0x80  // Previous command interrupted
#define APPLESMC_ST_1E_STILL_BAD_CMD    0x81  // Still processing bad command
#define APPLESMC_ST_1E_BAD_CMD          0x82  // Bad/invalid command
#define APPLESMC_ST_1E_NOEXIST          0x84  // Key does not exist
#define APPLESMC_ST_1E_WRITEONLY        0x85  // Key is write-only
#define APPLESMC_ST_1E_READONLY         0x86  // Key is read-only
#define APPLESMC_ST_1E_BAD_INDEX        0xB8  // Invalid index

// Timeout values (in microseconds)
#define SMC_STATUS_TIMEOUT_US   100000  // 100ms timeout for status wait
#define SMC_IO_DELAY_US         10      // 10μs delay between I/O operations

// Maximum data length for SMC keys
#define SMC_MAX_DATA_LENGTH     32

/**
 * Low-level I/O functions
 * Direct port access for SMC communication
 */

// Read byte from I/O port
UINT8 smc_inb(UINT16 port);

// Write byte to I/O port
void smc_outb(UINT16 port, UINT8 value);

/**
 * SMC Protocol Functions
 */

// Initialize SMC interface
EFI_STATUS smc_init(void);

// Detect if SMC is present
BOOLEAN smc_detect(void);

// Wait for specific status with timeout
EFI_STATUS smc_wait_status(UINT8 expected_status, UINT32 timeout_us);

// Read SMC key value
EFI_STATUS smc_read_key(const CHAR8 key[4], UINT8 *data, UINT8 *data_len);

// Write SMC key value
EFI_STATUS smc_write_key(const CHAR8 key[4], const UINT8 *data, UINT8 data_len);

// Get key type information
EFI_STATUS smc_get_key_type(const CHAR8 key[4], UINT8 *data_size, CHAR8 type[5]);

/**
 * Helper functions
 */

// Get last SMC error code
UINT8 smc_get_last_error(void);

// Clear SMC error status
void smc_clear_error(void);

#endif // SMC_PROTOCOL_H
