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
    if (!strncmp("fat32", args->file_system, strlen("fat32")))
        return 0;
    if (!strncmp("fat16", args->file_system, strlen("fat16")))
        return 0;
    if (!strncmp("fat12", args->file_system, strlen("fat12")))
        return 0;
    if (!strncmp("ntfs", args->file_system, strlen("ntfs")))
        return 0;
    fprintf(stderr, 
       "Aborting... invalid file system type: %s.  Please refer to the program usage for valid file system types.\n",
       args->file_system);
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

int read_mbr_table(int fp){
    int mbr_table_offsets[4] = {MBR_PART1_OFF, MBR_PART2_OFF, MBR_PART3_OFF, MBR_PART4_OFF};
    for (int i = 0; i < 4; i++){

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
    unsigned char buf[3];
    unsigned short mbr_sig = 0;
    unsigned int fs_type_sig = 0;

    // Begin checks for 0x55AA signature at offset 0x01FE
    if (pread(fp, buf, 2, MBR_SIG_OFF) < 0){
        read_error();
    }

    mbr_sig = (buf[0] << 8) | buf[1]; // OR both bytes into short
    if (mbr_sig != MBR_SIG){
        fprintf(stderr,
            "Aborting... %s does not appear to be a valid partition or MBR disk image.\n",
            args->image_path);
        exit(EXIT_FAILURE);
    }

    if (pread(fp, buf, 3, 0) < 0){
        read_error();
    }
    fs_type_sig = (buf[0] << 16) | (buf[1] << 8) | buf[2]; // OR three bytes into int
    
    switch (fs_type_sig){
        case NTFS_SIG:
            printf("Detected NTFS Partition.\n");
            return NTFS;
        case FAT32_SIG:
            printf("Detected FAT32 Partition.\n");
            return FAT32;
        case FAT_SIG:
            printf("Detected FAT12/FAT16 Partition.\n");
            return FAT16;
        default:
            printf("Detected a possible disk image with MBR\n");
            break;
    }

    return 0;
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
    char buffer[256];
    int fp = 0;
    read_args(&args, argc, argv);
    verify_fs_arg(&args);

    fp = open_disk_image(&args);

    verify_disk_image(fp, &args);

    CLEANUP:
    if (fp > 0)
        close(fp); // close file
}