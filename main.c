/** 
 * 
*/

#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

/** 
 * @brief: Parses command line arguments
 * @param[*args] 
 * @param[argc]
 * 
*/
int read_args(struct cmd_line *args, int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hvrs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h':
            args->h_flag = true;
            break;
        case 'v':
            args->v_flag = true;
            break;
        case 'r':
            args->r_flag = true;
            break;
        case 's':
            args->s_flag = true;
            args->s_value = (unsigned long)atoi(optarg);
            break;
        case 'E':
            args->E_flag = true;
            args->E_value = atoi(optarg);
            break;
        case 'b':
            args->b_flag = true;
            args->b_value = (unsigned long)atoi(optarg);
            break;
        case 't':
            args->t_flag = true;
            strncpy(args->t_path, optarg, 255);
            break;
        default:
            fprintf(stderr,
                    "Usage: %s [-hvr] -s <s> -E <E> -b <b> -t <tracefile>\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

int main(){

}