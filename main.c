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

    while ((opt = getopt(argc, argv, "i:f:t:")) != -1) {
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
        default:
            fprintf(stderr, "\nUsage: %s %s", argv[0], cmd_line_error);
            exit(EXIT_FAILURE);
        }
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
    if (!strncmp("raw", args->file_system, 4)){
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
    // malloc_mbr(&mbr, extended_found, &extended_entry);
    return 0;
}


/**
 * @brief 
 * 
 * @param fp 
 * @param mbr 
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
    if (pread(fp, &fat_sector->volume_serial, 4, partition_offset + RESERVED_AREA_SIZE) < 0)
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
    
    // If FAT32 is detected, read in extended FAT32 fields
    if (!fat_sector->fat_size_in_sectors){
        fat_sector->is_fat32 = true;

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
        if (pread(fp, &fat_sector->root_dir_cluster_addr, 4, partition_offset + ROOT_DIR_CLUSTER_ADDR) < 0)
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
    return 0;
}
/**
 * @brief Run a few checks to ensure partition data is valid
 * 
 */
void validate_fat_boot_sector(struct fat_boot_sector *fat_sector){
    // Check that Bytes Per Sector is Valid
    switch (fat_sector->bytes_per_sector)
    {
    case 512:
        break;
    case 1024:
        break;
    case 2048:
        break;
    case 4096:
        break;
    default:
        fprintf(stderr, "\nError!  Detected bytes per sector of: %d which is invalid.  \
        Must be 512, 1024, 2048, or 4096.  This indicates the disk image or file system might be corrupted", fat_sector->bytes_per_sector);
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
    if (sec_per_clus * fat_sector->bytes_per_sector > 32768){
        fprintf(stderr, "\nError!  Detected sector size of: %d which is invalid.  It must be a power of 2.  \
                This indicates the disk image or file system might be corrupted", sec_per_clus * fat_sector->bytes_per_sector);
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
    if (fat_sector->media_type != FIXED || fat_sector->media_type == REMOVABLE)
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
        case FAT_SIG:
            // printf("Detected FAT12/FAT16 Partition.\n\n");
            if (args->fs_type != FAT16 && args->fs_type != FAT12)
                fs_mismatch("fat12 or fat16");
            return FAT16;
        default:
            // printf("Detected a possible disk image with MBR (aka use -f raw)\n\n");
            if (args->fs_type != RAW)
                fs_mismatch("raw");
            return RAW;
            break;
    }

    return 0;
}

void print_fat_boot_sector_info(struct fat_boot_sector *fat_sector){
    printf("\nFAT File System Information\n\n");
    
    if (fat_sector->is_fat32)
        printf("File System Type: FAT32\n");
    else
        printf("File System Type: FAT12/16\n");

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
        printf("Volume Serial: %d\n", fat_sector->fat32_volume_serial);
        printf("Volume Label: %s\n", fat_sector->fat32_volume_label);
        printf("File System Label: %s\n", fat_sector->fat32_fs_type_label);
    }
    else{
        printf("Volume Serial: %d\n", fat_sector->volume_serial);
        printf("Volume Label: %s\n", fat_sector->volume_label);
        printf("File System Label: %s\n", fat_sector->fs_type_label);
    }
    printf("Bytes per sector: %d\n", fat_sector->bytes_per_sector);
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

    }
    else{
        printf("FAT size in sectors: %d\n", fat_sector->fat_size_in_sectors);
        printf("Maximum number of files in Root Dir: %d\n", fat_sector->max_files_in_root);
    }
}

void print_mbr_info(struct mbr_sector *mbr){
    // print out the headers first
    printf("%-8s %-4s %12s %12s %12s   %4s   %-25s\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6]);

    for (int i = 0; i < 4; i++){
        uint8_t boot_ind = mbr->entry[i].boot_indicator;
        uint8_t part_type = mbr->entry[i].partition_type;
        uint32_t starting_sector = mbr->entry[i].starting_sector;
        uint32_t partition_size = mbr->entry[i].partition_size;

        char bootable;

        if (boot_ind == 0)
            bootable = 'N';
        else
            bootable = 'Y';

        printf("%-8d %-4c %12ju %12ju %12ju   %#04x   %-25s\n", i, bootable, (uintmax_t)starting_sector, (uintmax_t)(starting_sector + partition_size), (uintmax_t)partition_size, part_type, partition_type_txt[part_type]);
    }
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
    struct mbr_sector* mbr = calloc(1, sizeof(struct mbr_sector));
    struct fat_boot_sector* fat_boot_sector;

    read_args(&args, argc, argv);
    verify_fs_arg(&args);

    fp = open_disk_image(&args);

    fs_type = verify_disk_image(fp, &args);

    if (fs_type == RAW){
        read_mbr_sector(fp, mbr);
        print_mbr_info(mbr);
    }

    if (fs_type == FAT32 || fs_type == FAT16 || fs_type == FAT12){
        fat_boot_sector = calloc(1, sizeof(struct fat_boot_sector));
        read_fat_boot_sector(fp, fat_boot_sector, 0);
        validate_fat_boot_sector(fat_boot_sector);
        print_fat_boot_sector_info(fat_boot_sector);
    }

    CLEANUP:
    if (fp > 0)
        close(fp); // close file
    if (mbr != NULL)
        free(mbr);
    if (fat_boot_sector != NULL)
        free(fat_boot_sector);
    //Need to add code to cleanup MBR Table structs
}