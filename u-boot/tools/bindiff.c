/* Very simple binary difference tool. */

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BLOCK_SIZE  4096

int main(int argc, char **argv)
{
    bool quiet = false;
    if (argc >= 2  &&  strcmp(argv[1], "-q") == 0)
    {
        quiet = true;
        argc -= 1;
        argv += 1;
    }
    
    if (2 <= argc  &&  argc <= 3)
    {
        int file1 = open(argv[1], O_RDONLY);
        int file2 = argc > 2 ? open(argv[2], O_RDONLY) : STDIN_FILENO;
        if (file1 == -1  ||  file2 == -1)
        {
            perror("Unable to open input file");
            return 2;
        }

        char file1_block[BLOCK_SIZE];
        char file2_block[BLOCK_SIZE];
        int file1_len, file2_len;

        while (
            file1_len = read(file1, file1_block, BLOCK_SIZE),
            file2_len = read(file2, file2_block, BLOCK_SIZE),
            file1_len > 0  &&  file2_len > 0)
        {
            if (file1_len == file2_len)
            {
                if (memcmp(file1_block, file2_block, file1_len) != 0)
                {
                    if (!quiet)
                        fprintf(stderr, "Files differ in content\n");
                    return 1;
                }
            }
            else
            {
                if (!quiet)
                    fprintf(stderr, "Files differ in length\n");
                return 1;
            }
        }

        if (file1_len == 0  &&  file2_len == 0)
            return 0;
        else if (file1_len < 0  ||  file2_len < 0)
        {
            perror("Error reading file");
            return 1;
        }
        else
        {
            if (!quiet)
                fprintf(stderr, "Files differ in length\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s [-q] file-one [file-two]\n", argv[0]);
        return 2;
    }
}
