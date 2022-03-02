/**
 * @file main.c
 * @author Nathan McFadden <nmcfadd2@andrew.cmu.edu>
 * @brief 
 * @version 0.1
 * @date 2022-03-02
 * 
 * 
 */

#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

char cmd_line_error[] = "-i <path_to_disk_image> -f <file_system_type>\n" \
                        "\nCurrently Supported file system types:\n <fat32>\n <raw> (MBR Disk Image with" \
                         "fat32 Partition)\n\n";

// Struct to store command line args
typedef struct cmd_line {
    // Booleans to specify if flag was present
    bool i_flag; // disk image path flag
    bool f_flag; // file system format flag

    // Flag values
    char image_path[255];
    char file_system[8];
} cmd_line;

/** 
 * @brief: Parses command line arguments
 * @param[*args] 
 * @param[argc]
 * 
*/
int read_args(struct cmd_line *args, int argc, char *argv[]) {
    int opt;
    if (argc == 1){ //runs if no cmd line arguments are provided
        fprintf(stderr, "\nUsage: %s %s", argv[0], cmd_line_error);
        exit(EXIT_FAILURE);
    }
    while ((opt = getopt(argc, argv, "i:f:")) != -1) {
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

int validate_filesystem_arg(struct cmd_line *args){
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
    return 1;
}

int main(int argc, char *argv[]){
    cmd_line args = {0};
    FILE *fp = 0;
    read_args(&args, argc, argv);

    if(validate_filesystem_arg(&args))
        exit(EXIT_FAILURE);

    fp = fopen(args.image_path, "r");
    // Ensure the file open was successful
    if (fp == NULL) {
        fprintf(stderr,
            "Aborting... Could not read/access the file located at: %s\n",
            args.image_path);
    }

    CLEANUP:
    if (fp != NULL)
        fclose(fp); // close file
}