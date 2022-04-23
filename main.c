/**
 * @file main.c
 * @author Nathan McFadden <nmcfadd2@andrew.cmu.edu>
 * @brief Feeler Gauge 
 * @version 0.1
 * @date 2022-03-02
 * 
 * 
 * Dependencies:
 *  - libmagic
 */

#include "main.h"

void read_error(void) {
    fprintf(stderr, "Unable to read disk image. Please make sure the file has not been moved or deleted.\n");
    exit(EXIT_FAILURE);
}


/**
 * @brief Convert Cluster to Sector
 * 
 * @return uint32_t 
 */
uint32_t cts(uint32_t cluster){
    return ((cluster - 2) * (spc * bps) + reserved_and_fats);
}

/**
 * @brief Parses cmd line arguments
 * 
 * @param args : struct to store cmd line arguments supplied by the user
 * @param argc : argument count (supplied by OS)
 * @param argv : string of cmd line arguments
 * @return int : return 0 if successful, 1 if an error occurs or invalid cmd line option
 */
int read_args(struct cmd_line *args, int argc, char *argv[]) {
    int opt;
    if (argc == 1){ //runs if no cmd line arguments are provided
        fprintf(stderr, "\nUsage: %s %s", argv[0], cmd_line_error);
        exit(EXIT_FAILURE);
    }

    strncpy(args->argv0, argv[0], 255);

    while ((opt = getopt(argc, argv, "i:f:v")) != -1) {
        switch (opt) {
        case 'i':
            args->i_flag = true;
            strncpy(args->image_path, optarg, 255);
            break;
        case 'f':
            args->f_flag = true;
            strncpy(args->file_system, optarg, 8);
            for(int i = 0; args->file_system[i]; i++){ //set file system input to lower case
                args->file_system[i] = tolower(args->file_system[i]);
            }
            break;
        case 'v':
            args->v_flag = true;
            break;
        default:
            fprintf(stderr, "\nUsage: %s %s", argv[0], cmd_line_error);
            exit(EXIT_FAILURE);
        }
    }
    if (args->i_flag == false){
        fprintf(stderr, "\nError! You must specify a disk image. < -i >\n");
    }
    if (args->f_flag == false){
        fprintf(stderr, "\nError! You must specify the format of the disk image.  < -f >\n");
    }
    if (args->f_flag == false || args->i_flag == false){
        fprintf(stderr, "\nUsage: %s %s", argv[0], cmd_line_error);
            exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * @brief Verfies that the user supplied a valid/support file system type.
 * @param args 
 * @return int : returns 0 if no errors, or 1 if invalid file sytem type detected
 */
int verify_fs_arg(struct cmd_line *args){
    if (!strncmp("fat32", args->file_system, 5)){
        args->fs_type = FAT32;
        return 0;
    }
    if (!strncmp("fat16", args->file_system, 5)){
        args->fs_type = FAT16;
        return 0;
    }
    if (!strncmp("fat12", args->file_system, 5)){
        args->fs_type = FAT12;
        return 0;
    }
    if (!strncmp("ntfs", args->file_system, 4)){
        args->fs_type = NTFS;
        return 0;
    }
    if (!strncmp("raw", args->file_system, 3)){
        args->fs_type = RAW;
        return 0;
    }

    fprintf(stderr, 
       "Aborting... invalid file system type: %s.  Please refer to the program usage for valid file system types.\n",
       args->file_system);
       fprintf(stderr, "\nUsage: %s %s\n", args->argv0, cmd_line_error);
    exit(EXIT_FAILURE);
}

/**
 * @brief Attempts to open the disk image supplied by the user.
 * 
 * @param args : struct containing the various cmd line arguments supplied by the user
 * @return int : return fp if successful, -1 if an error occurs
 */
int open_disk_image(struct cmd_line *args){
    int fp = open(args->image_path, O_RDONLY);
    // Ensure the file open was successful
    if (fp == -1) {
        fprintf(stderr,
            "Aborting... Could not read/access the file located at: %s\n",
            args->image_path);
        exit(EXIT_FAILURE);
    }
    return fp;
}

int read_mbr_sector(int fp, struct mbr_sector *mbr){
    int mbr_sector_offsets[4] = {MBR_PART1_OFF, MBR_PART2_OFF, MBR_PART3_OFF, MBR_PART4_OFF};
    uint8_t buf[1] = {0};
    uint32_t buf32[1] = {0};
    bool extended_found = false;
    uint8_t extended_entry[4] = {0};

    // Parse MBR
    for (int i = 0; i < 4; i++){
        // Check for extended partitions within MBR
        if (pread(fp, buf, 1, mbr_sector_offsets[i] + PARTITION_TYPE) < 0)
            read_error();
        if (buf[0] == EXTENDED || buf[0] == EXTENDED_LBA){
            extended_found = true;
            extended_entry[i] = true;
        }

        // Get Partiton Type
        if (pread(fp, buf, 1, mbr_sector_offsets[i] + PARTITION_TYPE) < 0)
            read_error();
        mbr->entry[i].partition_type = buf[0];

        // Get Boot Indicator Status
        if (pread(fp, buf, 1, mbr_sector_offsets[i] + BOOT_INDICATOR) < 0)
            read_error();
        mbr->entry[i].boot_indicator = buf[0];

        // Get Starting Sector
        if (pread(fp, buf32, 4, mbr_sector_offsets[i] + STARTING_SECTOR) < 0)
            read_error();
        mbr->entry[i].starting_sector = buf32[0];

        // Get Partition Size
        if (pread(fp, buf32, 4, mbr_sector_offsets[i] + PARTITION_SIZE) < 0)
            read_error();
        mbr->entry[i].partition_size = buf32[0];
    }
    
    // Will be used once extended entry support is added
    if (extended_found){
        for (int i = 0; i < 4; i++){
            if (extended_entry[i])
                continue;
        }
    }
    return 0;
}


/**
 * @brief Runs a series of calculations to determine FAT File System Type.  Can differentiate
 * between FAT12, FAT16, and FAT32.  Formula based on page 229 of File System Forensic Analysis 
 * book by Brian Carrier
 * 
 * @param fat_sector 
 */
void calc_fat_type(struct fat_boot_sector *fat_sector){
    uint32_t root_dir_sectors = ((fat_sector->max_files_in_root * 32) + (bps - 1)) / bps;
    uint32_t sectors_to_clusters;
    if (fat_sector->sector_count_16b)
        sectors_to_clusters = fat_sector->sector_count_16b - fat_sector->reserved_area_size - (fat_sector->number_of_fats * fat_sector->fat_size_in_sectors) - root_dir_sectors;
    else
        sectors_to_clusters = fat_sector->sector_count_32b - fat_sector->reserved_area_size - (fat_sector->number_of_fats * fat_sector->fat_size_in_sectors) - root_dir_sectors;
    
    uint32_t final_value = sectors_to_clusters/fat_sector->sectors_per_cluster;

    if (final_value < 4085)
        fat_sector->is_fat12 = true;
    if (final_value >= 4085 && final_value < 65525)
        fat_sector->is_fat16 = true;
    if (final_value >= 65525)
        fat_sector->is_fat32 = true;
    // printf("Sectors in Root Dir: %d\n", root_dir_sectors);
    // printf("Sectors allocated to clusters: %ju\n", sectors_to_clusters/fat_sector->sectors_per_cluster);
}

/**
 * @brief 
 * 
 * @param fp 
 * @param mbr
 * @param partition_offset If a raw/full disk image is used, this is
 * the offset within the disk image to the FAT boot sector
 * @return int 
 */
int read_fat_boot_sector(int fp, struct fat_boot_sector *fat_sector, int partition_offset){
    char str_buf[12];

    // Get OEM Name
    if (pread(fp, str_buf, 8, partition_offset + OEM_NAME) < 0)
        read_error();
    strncpy(fat_sector->oem_name, str_buf, 8);

    // Get Bytes Per Sector
    if (pread(fp, &fat_sector->bytes_per_sector, 2, partition_offset + BYTES_PER_SECTOR) < 0)
        read_error();
    
    // Get Sectors Per Cluster
    if (pread(fp, &fat_sector->sectors_per_cluster, 1, partition_offset + SECTORS_PER_CLUSTER) < 0)
        read_error();
    
    // Get Reserved Area Size
    if (pread(fp, &fat_sector->reserved_area_size, 2, partition_offset + RESERVED_AREA_SIZE) < 0)
        read_error();
    
    // Get Number of Fats
    if (pread(fp, &fat_sector->number_of_fats, 1, partition_offset + NUMBER_OF_FATS) < 0)
        read_error();
    
    // Get Max Files in Root
    if (pread(fp, &fat_sector->max_files_in_root, 2, partition_offset + MAX_FILES_IN_ROOT) < 0)
        read_error();
    
    // Get Sector Count
    if (pread(fp, &fat_sector->sector_count_16b, 2, partition_offset + SECTOR_COUNT_16B) < 0)
        read_error();
    
    // Get Media Type
    if (pread(fp, &fat_sector->media_type, 1, partition_offset + MEDIA_TYPE) < 0)
        read_error();
    
    // Get Fat Size in Sectors
    if (pread(fp, &fat_sector->fat_size_in_sectors, 2, partition_offset + FAT_SIZE_IN_SECTORS) < 0)
        read_error();
    
    // Get Sectors Per Track
    if (pread(fp, &fat_sector->sectors_per_track, 2, partition_offset + SECTORS_PER_TRACK) < 0)
        read_error();
    
    // Get Number of Heads
    if (pread(fp, &fat_sector->head_number, 2, partition_offset + HEAD_NUMBER) < 0)
        read_error();
    
    // Get Sectors Before Partition
    if (pread(fp, &fat_sector->sectors_before_partition, 4, partition_offset + SECTORS_BEFORE_PARTITION) < 0)
        read_error();
    
    // Get Sector Count FAT32
    if (pread(fp, &fat_sector->sector_count_32b, 4, partition_offset + SECTOR_COUNT_32B) < 0)
        read_error();
    
    // Get BIOS Drive Number
    if (pread(fp, &fat_sector->bios_drive_number, 1, partition_offset + BIOS_DRIVE_NUMBER) < 0)
        read_error();
    
    // Get Extended Boot Signature
    if (pread(fp, &fat_sector->extended_boot_sig, 1, partition_offset + EXTENDED_BOOT_SIG) < 0)
        read_error();
    
    // Get Volume Serial
    if (pread(fp, &fat_sector->volume_serial, 4, partition_offset + VOLUME_SERIAL) < 0)
        read_error();
    
    // Get Volume Label
    if (pread(fp, str_buf, 11, partition_offset + VOLUME_LABEL) < 0)
        read_error();
    strncpy(fat_sector->volume_label, str_buf, 11);

    // Get File System Label
    if (pread(fp, str_buf, 8, partition_offset + FS_TYPE_LABEL) < 0)
        read_error();
    strncpy(fat_sector->fs_type_label, str_buf, 8);

    // Get File System Signature
    if (pread(fp, &fat_sector->fs_signature, 2, partition_offset + FS_SIGNATURE) < 0)
        read_error();

    //Write Global VAR 'bps' - shortcut for Bytes Per Sector
    bps = fat_sector->bytes_per_sector;

    //Write Global VAR 'spc' - shortcut for Sectors Per Cluster
    spc = fat_sector->sectors_per_cluster;


    // Determine FAT Type (i.e. FAT12, FAT16, or FAT32)
    calc_fat_type(fat_sector);

    // If FAT32 is detected, read in extended FAT32 fields
    if (fat_sector->is_fat32){

        // Get FAT32 Size in Sectors
        if (pread(fp, &fat_sector->fat32_size_in_sectors, 4, partition_offset + FAT32_SIZE_IN_SECTORS) < 0)
            read_error();
        
        // Get FAT Mode
        if (pread(fp, &fat_sector->fat_mode, 2, partition_offset + FAT_MODE) < 0)
            read_error();

        // Get FAT32 Version
        if (pread(fp, &fat_sector->fat32_version, 2, partition_offset + FAT32_VERSION) < 0)
            read_error();
        
        // Get Root Dir Cluster
        if (pread(fp, &fat_sector->root_dir_cluster, 4, partition_offset + ROOT_DIR_CLUSTER) < 0)
            read_error();
        
        // Get FSINFO
        if (pread(fp, &fat_sector->fsinfo_sector_addr, 2, partition_offset + FSINFO_SECTOR) < 0)
            read_error();
        
        // Get Backup Boot Sector Addr
        if (pread(fp, &fat_sector->backup_boot_sector_addr, 2, partition_offset + BACKUP_BOOT_SECTOR_ADDR) < 0)
            read_error();
        
        // Get FAT32 BIOS Drive Number
        if (pread(fp, &fat_sector->fat32_bios_drive_number, 1, partition_offset + FAT32_BIOS_DRIVE_NUMBER) < 0)
            read_error();
        
        // Get FAT32 Extended Boot Sig
        if (pread(fp, &fat_sector->fat32_extended_boot_sig, 1, partition_offset + FAT32_EXTENDED_BOOT_SIG) < 0)
            read_error();
        
        // Get FAT32 Volume Serial
        if (pread(fp, &fat_sector->fat32_volume_serial, 4, partition_offset + FAT32_VOLUME_SERIAL) < 0)
            read_error();
        
        // Get FAT32 Volume Label
        if (pread(fp, str_buf, 11, partition_offset + FAT32_VOLUME_LABEL) < 0)
            read_error();
        strncpy(fat_sector->fat32_volume_label, str_buf, 11);

        // Get FAT32 File System Label
        if (pread(fp, str_buf, 8, partition_offset + FAT32_FS_TYPE_LABEL) < 0)
            read_error();
        strncpy(fat_sector->fat32_fs_type_label, str_buf, 8);
    }

    //Write Global VAR 'reserved_and_fats'
    if (fat_sector->is_fat32)
        reserved_and_fats = (fat_sector->reserved_area_size * bps) + (fat_sector->fat32_size_in_sectors * bps * fat_sector->number_of_fats);
    if (fat_sector->is_fat16)
        reserved_and_fats = (fat_sector->reserved_area_size * bps) + (fat_sector->fat_size_in_sectors * bps * fat_sector->number_of_fats);
    
    return 0;
}
/**
 * @brief Run a few checks to ensure partition data is valid
 * 
 */
void validate_fat_boot_sector(struct fat_boot_sector *fat_sector){
    // Check that Bytes Per Sector is Valid
    switch (bps){
        case 512:
            break;
        case 1024:
            break;
        case 2048:
            break;
        case 4096:
            break;
        default:
            fprintf(stderr, "\nError!  Detected bytes per sector of: %d which is invalid."  \
            "Must be 512, 1024, 2048, or 4096.  This indicates the disk image or file system might be corrupted", 
            bps);
            exit(EXIT_FAILURE);
            break;
    }

    // Check that Sectors Cluster is a Power of 2 and less than 32KB
    uint8_t sec_per_clus = fat_sector->sectors_per_cluster;
    if(!(sec_per_clus != 0) && ((sec_per_clus &(sec_per_clus - 1)) == 0)){
        fprintf(stderr, "\nError!  Detected sectors per cluster of: %d which is invalid.  It must be a power of 2.  \
                This indicates the disk image or file system might be corrupted", sec_per_clus);
        exit(EXIT_FAILURE);
    }
    if (sec_per_clus * bps > 32768){
        fprintf(stderr, "\nError!  Detected sector size of: %d which is invalid.  It must be a power of 2.  \
                This indicates the disk image or file system might be corrupted", sec_per_clus * bps);
        exit(EXIT_FAILURE);
    }

    // Check Number of FATS > 0
    if (fat_sector->number_of_fats < 1){
        fprintf(stderr, "\nError!  No FATs found. This indicates the disk image or file system might be corrupted");
        exit(EXIT_FAILURE);
    }
    
    // Check that Max Number of Files in Root Director is 0 when FAT32 detected
    if (fat_sector->max_files_in_root && !fat_sector->fat_size_in_sectors){
        fprintf(stderr, "\nWarning!  Conflicting indicators for FAT12/16 and FAT32.  The disk image or file system might be corrupted, proceed with caution.");
    }

    // Check media type
    if (fat_sector->media_type != FIXED && fat_sector->media_type != REMOVABLE)
        fprintf(stderr, "\nWarning!  Media type (removable/fixed) could not be detected.  The disk image or file system might be corrupted, proceed with caution.");

    // Check that only one sector count is present (16 bits for FAT12/FAT16, or 32 bits for FAT32)
    if (fat_sector->sector_count_16b && fat_sector->sector_count_32b){
        fprintf(stderr, "\nWarning!  Conflicting sector counts (both 16 bit and 32 bit fields contained values).  This tool will continue using the 32 bit sector count, but the disk image or file system might be corrupted, proceed with caution.");
    }
}

/**
 * @brief Checks supplied disk image to ensure 0x55AA signature found, and then attempts to
 * determine if the disk is a full disk image (i.e. still has MBR), or is just an image of a 
 * single file system/partition
 * 
 * @param fp 
 * @param args 
 * @return int : return 0 if disk image with MBR detected, return file system enum if detected
 */
int verify_disk_image(int fp, struct cmd_line *args){
    uint8_t buf[3];
    unsigned short mbr_sig = 0;
    unsigned int fs_type_sig = 0;

    void fs_mismatch(char fs_type[15]){
        fprintf(stderr, "Detected File System: %s\n", fs_type);
        fprintf(stderr,
            "Aborting... Detected file system type does not match your -f command line argument: %s\n",
            args->file_system);
        fprintf(stderr, "\nUsage: %s %s\n", args->argv0, cmd_line_error);
        exit(EXIT_FAILURE);
    }

    // Begin checks for 0x55AA signature at offset 0x01FE
    if (pread(fp, buf, 2, MBR_SIG_OFF) < 0)
        read_error();

    mbr_sig = (buf[0] << 8) | buf[1]; // OR both bytes into short
    if (mbr_sig != MBR_SIG){
        fprintf(stderr,
            "Aborting... %s does not appear to be a valid partition or MBR disk image.\n",
            args->image_path);
        exit(EXIT_FAILURE);
    }

    // Read the File System Signature at Offset 0
    if (pread(fp, buf, 3, 0) < 0)
        read_error();

    // File system signatures are 3 bytes
    fs_type_sig = (buf[0] << 16) | (buf[1] << 8) | buf[2]; // Combine three bytes into int
    
    switch (fs_type_sig){
        case NTFS_SIG:
            // printf("Detected NTFS Partition.\n\n");
            if (args->fs_type != NTFS)
                fs_mismatch("ntfs");
            return NTFS;
        case FAT32_SIG:
            // printf("Detected FAT32 Partition.\n\n");
            if (args->fs_type != FAT32)
                fs_mismatch("fat32");
            return FAT32;
        case FAT16_SIG:
            // printf("Detected FAT12/FAT16 Partition.\n\n");
            if (args->fs_type != FAT16 )
                fs_mismatch("fat16");
            return FAT16;
        case FAT12_SIG:
            // printf("Detected FAT12/FAT16 Partition.\n\n");
            if (args->fs_type != FAT12 )
                fs_mismatch("fat12");
            return FAT12;
        default:
            // printf("Detected a possible disk image with MBR (aka use -f raw)\n\n");
            if (args->fs_type != RAW)
                fs_mismatch("raw");
            return RAW;
            break;
    }

    return 0;
}

/**
 * @brief Print FAT Tables
 * I know, this code is horrible.  Formatting tables with variable widths based on file system type is hard.
 * It works for now.....
 */
void print_full_fat_tables(uint8_t* fat1_ptr, uint8_t* fat2_ptr, struct fat_boot_sector *fat_sector){
    bool empty_row = false;
    bool printing_empty_block = false;
    char dash[] = "-----------------------------------------------------------------------------------------------------------";
    char space[] = "                                                                                                                       ";
    char fat32_banner[] = "            |                                             FAT 1 (FAT32)                                             | \n";
    char fat16_banner[] = "            |                             FAT 1 (FAT16)                             | \n";
    char *fat_banner;
    uint32_t fat_entries = 0;
    uint16_t *fat1_16 = (uint16_t *)fat1_ptr;
    // uint16_t *fat2_16 = (uint16_t *)fat2_ptr;
    uint32_t *fat1_32 = (uint32_t *)fat1_ptr;
    // uint32_t *fat2_32 = (uint32_t *)fat2_ptr;
    uint32_t i = 0;

    int width = 0;
    int dash_width = 0;
    int space_width = 0;
    if (fat_sector->is_fat16){
        width = 4;
        dash_width = 71;
        space_width = 11;
        fat_entries = fat_sector->fat_size_in_sectors * fat_sector->bytes_per_sector / 16;
        fat_banner = fat16_banner;
    }
    if (fat_sector->is_fat32){
        width = 8;
        dash_width = 103;
        space_width = 27;
        fat_entries = fat_sector->fat32_size_in_sectors * fat_sector->bytes_per_sector / 32;
        fat_banner = fat32_banner;
    }

    if (fat_sector->is_fat12){
        //Need to implement special logic for fat12 since 12 bits does not fit cleanly into a normal c datatype.
        return;
    }


    void print_dash(){
        printf("%.*s", 13, space);
        printf("%.*s\n", dash_width, dash);
    }

    print_dash();
    printf("%s", fat_banner);
    for (; i < fat_entries; i += 8){
        if (!printing_empty_block){
            for (uint8_t j = 0; j < 8; j++){
                if (i+j < fat_entries){
                    if (fat_sector->is_fat32)
                        empty_row = empty_row || fat1_32[i+j];
                    if (fat_sector->is_fat16)
                        empty_row = empty_row || fat1_16[i+j];
                }
            }
            empty_row = !empty_row;

            if (!empty_row && !printing_empty_block){
                print_dash();
                printf(" 0x%08x |", i);
                for (uint8_t j = 0; j < 8; j++){
                    if (i+j < fat_entries){
                        if (fat_sector->is_fat32)
                            printf(" 0x%0*x |", width, fat1_32[i+j]);
                        if (fat_sector->is_fat16)
                            printf(" 0x%0*x |", width, fat1_16[i+j]);
                    }
                }
                printf("\n");
                continue;
            }
            if (empty_row && !printing_empty_block){
                printing_empty_block = true;
                continue;
            }
            if (!empty_row && printing_empty_block){
                printing_empty_block = false;
                printf("<Block of Empty/Zero FAT Entries>");
                print_dash();
                printf(" 0x%08x |", i);
                for (uint8_t j = 0; j < 8; j++){
                    if (i+j < fat_entries){
                        if (fat_sector->is_fat32)
                            printf(" 0x%0*x |", width, fat1_32[i+j]);
                        if (fat_sector->is_fat16)
                            printf(" 0x%0*x |", width, fat1_16[i+j]);
                    }
                }
                printf("\n");
                continue;
            }
        }
    }
    print_dash();
    if (printing_empty_block){
        printf("            |");
        printf("%.*s", space_width, space);
        printf("Contiguous Block of Empty/Unallocated FAT Entries");
        printf("%.*s|\n", space_width, space);
        print_dash();
    }
    printf(" 0x%08x |", i); // print last address of FAT
    printf("%.*s", space_width, space);
    printf("                    End of FAT                   ");
    printf("%.*s|\n", space_width, space);
    print_dash();

}

/**
 * @brief Prints out information parsed from Fat Boot Sector
 * 
 * @param fat_sector 
 */
void print_fat_boot_sector_info(struct fat_boot_sector *fat_sector){
    printf("\nFAT File System Information\n\n");
    
    if (fat_sector->is_fat32)
        printf("File System Type: FAT32\n");
    if (fat_sector->is_fat16)
        printf("File System Type: FAT16\n");
    if (fat_sector->is_fat12)
        printf("File System Type: FAT12\n");

    switch (fat_sector->media_type){
        case FIXED:
            printf("Media Type: Fixed\n");
            break;
        case REMOVABLE:
            printf("Media Type: Removable\n");
            break;
        default:
            printf("Media Type: Unknown\n");
            break;
    }
    
    printf("OEM Name: %s\n", fat_sector->oem_name);
    if(fat_sector->is_fat32){
        printf("Volume Serial: 0x%zx\n", (size_t)fat_sector->fat32_volume_serial);
        printf("Volume Label: %s\n", fat_sector->fat32_volume_label);
        printf("File System Label: %s\n", fat_sector->fat32_fs_type_label);
    }
    else{
        printf("Volume Serial: 0x%zx\n", (size_t)fat_sector->volume_serial);
        printf("Volume Label: %s\n", fat_sector->volume_label);
        printf("File System Label: %s\n", fat_sector->fs_type_label);
    }
    printf("Bytes per sector: %d\n", bps);
    printf("Sectors per cluster: %d\n", fat_sector->sectors_per_cluster);
    printf("Size of Reserved Area (in sectors): %d\n", fat_sector->reserved_area_size);
    printf("Number of FATs: %d\n", fat_sector->number_of_fats);
    
    if (fat_sector->sector_count_32b)
        printf("Number of sectors: %d\n", fat_sector->sector_count_32b);
    else
        printf("Number of sectors: %d\n", fat_sector->sector_count_16b);

    printf("Sectors before start of partition: %d\n", fat_sector->sectors_before_partition);
    
    if(fat_sector->is_fat32){
        printf("FAT size in sectors: %d\n", fat_sector->fat32_size_in_sectors);
        printf("Root Dir Cluster: %d\n", fat_sector->root_dir_cluster);
    }
    else{
        printf("FAT size in sectors: %d\n", fat_sector->fat_size_in_sectors);
        printf("Maximum number of files in Root Dir: %d\n", fat_sector->max_files_in_root);
    }
}

/**
 * @brief Prints out information parsed from MBR
 * 
 * @param mbr 
 */
void print_mbr_info(struct mbr_sector *mbr){
    // print out the headers first
    printf("%-8s %-4s %12s %12s %12s   %4s   %-25s\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6]);

    for (int i = 0; i < 4; i++){
        char bootable;

        if (mbr->entry[i].boot_indicator == 0)
            bootable = 'N';
        else
            bootable = 'Y';

        printf("%-8d %-4c %12ju %12ju %12ju   %#04x   %-25s\n", 
        i, bootable, (uintmax_t)mbr->entry[i].starting_sector, 
        (uintmax_t)(mbr->entry[i].starting_sector + mbr->entry[i].partition_size), 
        (uintmax_t)mbr->entry[i].partition_size, mbr->entry[i].partition_type, 
        partition_type_txt[mbr->entry[i].partition_type]);
    }
}

/**
 * @brief Copies the FATs from the disk image into memory, and then compares them to see
 * if there are any differences between FAT1 and FAT2
 * 
 * @param fp file pointer to disk image
 * @param fs_type type of file system (enum)
 * @param fat_boot_sector 
 * @param fat1 
 * @param fat2 
 */
void copy_fats_into_memory(int fp, int fs_type, struct fat_boot_sector* fat_sector, uint8_t **fat1_ptr, uint8_t **fat2_ptr){
    uint64_t diff = 0;
    uint32_t reserved_area_size_in_bytes = 0;
    uint32_t fat_size_in_bytes = 0;

    reserved_area_size_in_bytes = fat_sector->reserved_area_size * bps;

    if (fs_type == FAT32)
        fat_size_in_bytes = fat_sector->fat32_size_in_sectors * bps;
    else
        fat_size_in_bytes = fat_sector->fat_size_in_sectors * bps;

    uint8_t *fat1 = malloc(fat_size_in_bytes);
    uint8_t *fat2 = malloc(fat_size_in_bytes);
    *fat1_ptr = fat1;
    *fat2_ptr = fat2;

    if (pread(fp, fat1, fat_size_in_bytes, reserved_area_size_in_bytes) < 0)
        read_error();
    if (pread(fp, fat2, fat_size_in_bytes, reserved_area_size_in_bytes + fat_size_in_bytes) < 0)
        read_error();

    for(int i = 0; i < fat_size_in_bytes; i++){
        if (fat1[i] ^ fat2[i]){
            diff++;
            if (diff <= 10){
                printf("Detected discrepency between FAT1 and FAT2 at the following offsets.  FAT1: %#2x, FAT2: %#02x\n", 
                reserved_area_size_in_bytes + i, reserved_area_size_in_bytes + fat_size_in_bytes + i);
            }
        }
        if (diff == 11)
            printf("More than 10 discrepencies between FAT1 and FAT2 detected.  To reduce output clutter, individual discrepencies will no longer be printed.\n");
    }
    if (diff > 0)
        printf("Total # of discrepencies identified between FAT1 and FAT2: %ju\n", diff);
}


/**
 * @brief Function traverses Long File Name (LFN) entires within the FAT32 file system to find the Short
 * File Name (SFN) entry which actually contains the information like time stamps, size, and first cluster.
 * 
 * @param fp File Pointer to the disk image
 * @return uint32_t The offset to the short file name record
 */
uint32_t traverse_lfn_entries(int fp, uint32_t offset){
    uint32_t current_lfn_offset = 0;
    uint32_t current_entry_attribute = 0;
    do{
        if (pread(fp, &current_entry_attribute, 1, offset + FILE_ATTRIBUTES + current_lfn_offset) < 0)
            read_error();
        current_lfn_offset += 32; //increment to the next directory entry
    } while (current_entry_attribute == FLAG_FAT_LONG_FILE_NAME);
    return (current_lfn_offset - 32);
}

/**
 * @brief Returns the value stored within a given FAT table entry
 * 
 * @param cluster entry to be read 
 * @return uint32_t 
 */

uint32_t read_fat_entry(uint32_t cluster){
    if (fat_bs->is_fat32){
        uint32_t *fat32 = (uint32_t *) fat1;
        return fat32[cluster];
    }
    if (fat_bs->is_fat16){
        uint16_t *fat16 = (uint16_t *) fat1;
        return fat16[cluster];
    }
    return 0;
}

/**
 * @brief Loads a fat_dir_entry struct with directory entry info
 * 
 * @param fp file pointer to disk image
 * @param entry pointer to entry struct to store read information
 * @param offset 
 * @return uint32_t 
 */
uint32_t read_fat_dir_entry(int fp, struct fat_dir_entry *entry, uint32_t offset){
    // Traverse the LFN entries to get to the SFN entry
    uint32_t LFN = traverse_lfn_entries(fp, offset);

    if (pread(fp, entry->info.filename, 11, offset + LFN + FILE_NAME) < 0)
        read_error();
    if (pread(fp, &entry->file_attributes, 1, offset + LFN + FILE_ATTRIBUTES) < 0)
        read_error();
    if (pread(fp, &entry->created_time_tenths, 1, offset + LFN + CREATED_TIME_TENTHS) < 0)
        read_error();
    if (pread(fp, &entry->created_time_hms, 2, offset + LFN + CREATED_TIME_HMS) < 0)
        read_error();
    if (pread(fp, &entry->created_day, 2, offset + LFN + CREATED_DAY) < 0)
        read_error();
    if (pread(fp, &entry->accessed_day, 2, offset + LFN + ACCESSED_DAY) < 0)
        read_error();
    if (pread(fp, &entry->low_cluster_addr, 2, offset + LFN + LOW_CLUSTER_ADDR) < 0)
        read_error();
    if (pread(fp, &entry->high_cluster_addr, 2, offset + LFN + HIGH_CLUSTER_ADDR) < 0)
        read_error();

    entry->cluster_addr = entry->low_cluster_addr | (entry->high_cluster_addr << 16);
    
    if (pread(fp, &entry->written_time_hms, 2, offset + LFN + WRITTEN_TIME_HMS) < 0)
        read_error();
    if (pread(fp, &entry->written_day, 2, offset + LFN + WRITTEN_DAY) < 0)
        read_error();
    if (pread(fp, &entry->file_size, 4, offset + LFN + FILE_SIZE) < 0)
        read_error();
    return LFN + 32;
}

/**
 * @brief Returns the number of clusters a file or directory takes up on the disk.  It does this by
 * traversing the FAT until it hits an EOF marker.
 * 
 * @param cluster 
 * @return uint32_t 
 */
uint32_t get_entry_size(uint32_t cluster){
    uint32_t next_cluster = cluster;
    uint32_t size = 1;

    if(fat_bs->is_fat32){
        do{
            next_cluster = read_fat_entry(next_cluster);
            size++;
        } while (next_cluster < (uint32_t)FAT32_EOF);
    }
    return size;
}

/**
 * @brief Returns the clusters a file or directory occupies up on the disk.  It does this by
 * traversing the FAT until it hits an EOF marker.
 * 
 * @param cluster The starting cluster
 * @param cluster_list A poiner to a block/array to store the list of clusters
 * @param max_length Since we pre-allocated memory for the list, this is a safety check to avoid overrunning the buffer
 * @return uint32_t 
 */
void get_fat32_entry_clusters(uint32_t cluster, uint32_t *cluster_list, uint32_t max_length){
    uint32_t next_cluster = cluster;
    uint32_t size = 0;
    do{
        cluster_list[size] = next_cluster;
        next_cluster = read_fat_entry(next_cluster);
        //printf("Next Cluster: %zu\n", next_cluster);
        size++;
    } while (next_cluster < FAT32_EOF && size < max_length);
}

struct fat_dir_entry* read_fat32_filesystem(int fp, uint32_t entry_start_cluster){    
    uint32_t dir_size = get_entry_size(entry_start_cluster);
    uint32_t *dir_cluster_list = calloc(dir_size, sizeof(uint32_t));
    struct fat_dir_entry *entry = calloc(1, sizeof(struct fat_dir_entry));
    get_fat32_entry_clusters(entry_start_cluster, dir_cluster_list, dir_size);
    
    entry->last_cluster = dir_cluster_list[dir_size-1];


    free(dir_cluster_list);
    return entry;
}


/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[]){
    cmd_line args = {0};
    int fp = 0;
    int fs_type = 0;
    root_dir_off = 0;
    struct mbr_sector* mbr = calloc(1, sizeof(struct mbr_sector));

    read_args(&args, argc, argv);
    verify_fs_arg(&args);

    fp = open_disk_image(&args);

    fs_type = verify_disk_image(fp, &args);

    if (fs_type == RAW){
        read_mbr_sector(fp, mbr);
        print_mbr_info(mbr);
    }

    if (fs_type == FAT32 || fs_type == FAT16 || fs_type == FAT12){
        fat_bs = calloc(1, sizeof(struct fat_boot_sector));
        struct fat_dir_entry *root_dir;
        read_fat_boot_sector(fp, fat_bs, 0);
        validate_fat_boot_sector(fat_bs);
        print_fat_boot_sector_info(fat_bs);
        copy_fats_into_memory(fp, fs_type, fat_bs, &fat1, &fat2);
        
        if (args.v_flag == true) //print fat table in verbose mode
            print_full_fat_tables(fat1, fat2, fat_bs);

        if(fs_type == FAT32){
            root_dir_off = cts(fat_bs->root_dir_cluster);
            cluster2_off = reserved_and_fats;
            printf("Starting to read Fat32 filesystem.\n");
            root_dir = read_fat32_filesystem(fp, fat_bs->root_dir_cluster);
        }
        if(fs_type == FAT16){
            root_dir_off = fat_bs->number_of_fats * (fat_bs->fat_size_in_sectors * bps) + (fat_bs->reserved_area_size * bps);
            cluster2_off = reserved_and_fats + fat_bs->max_files_in_root * 32;
        }
    }

    CLEANUP:
    if (fp > 0)
        close(fp); // close file
    if (mbr != NULL)
        free(mbr);
    if (fat_bs != NULL)
        free(fat_bs);
    if (fat1 != NULL)
        free(fat1);
    if (fat2 != NULL)
        free(fat2);
    
    //Need to add code to cleanup MBR Table structs
}