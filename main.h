#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/magic.h> // Used for file header/footer magic numbers

const char cmd_line_error[] = "-i <path_to_disk_image> -f <file_system_type>\n" \
                        "\nCurrently Supported file system types:\n <fat32>\n <raw> (MBR Disk Image with" \
                         "fat32 Partition)\n\n";

/**
 * @brief Common partition type codes for MBR entries
 */
enum partition_type {
    FAT12 = 0x1,
    FAT16 = 0x4,
    FAT32_CHS = 0x0B,
    FAT32 = 0x0C, //FAT32 with LBA
    EXTENDED_LBA = 0x0F,
    NTFS = 0x7,
    LINUX_SWAP = 0x82,
    LINUX_FILE_SYS = 0x83,
    EMPTY_ENTRY = 0x00
};

/**
 * @brief Common offsets within MBR table
 */
enum offsets {
    MBR_SIG_OFF = 0x01FE, // MBR Signature Field Offset
    MBR_PART1_OFF = 0x01BE, // MBR Partition 1 Field Offset
    MBR_PART2_OFF = 0x01CE, // MBR Partition 2 Field Offset
    MBR_PART3_OFF = 0x01DE, // MBR Partition 3 Field Offset
    MBR_PART4_OFF = 0x01EE, // MBR Partition 4 Field Offset
    
    // Partition Table Entry Relative Offsets
    BOOT_INDICATOR = 0,
    PARTITION_TYPE = 1,
    STARTING_SECTOR = 8,
    PARTITION_SIZE = 12
};

/**
 * @brief 
 */
enum signatures {
    MBR_SIG = 0x55AA,
    NTFS_SIG = 0xEB5290,
    FAT_SIG = 0xEB3C90,
    FAT32_SIG = 0xEB5890
};

// Struct to store command line args
typedef struct cmd_line {
    // Booleans to specify if flag was present
    bool i_flag; // disk image path flag
    bool f_flag; // file system format flag

    // Flag values
    char image_path[255];
    char file_system[8];

} cmd_line;

// Struct to store command line args
typedef struct partition_table {
    // Booleans to specify if flag was present
    unsigned char boot_indicator;
    unsigned char partition_type;
    unsigned int starting_sector;
    unsigned int partition_size;  // size in sectors
} partition_table;

typedef struct mbr_table {
    struct partition_table entry1;
    struct partition_table entry2;
    struct partition_table entry3;
    struct partition_table entry4;
} mbr_table;