#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern unsigned long crc32 (
    unsigned long, const unsigned char *, unsigned int);


#define BLOCK_SIZE 4096

void print_crc32(char *filename)
{
    int input = open(filename, O_RDONLY);
    if (input == -1)
    {
        perror("Error opening file");
        return;
    }

    unsigned char block[BLOCK_SIZE];
    unsigned int crc = 0;
    int total_length = 0;
    int count;
    while (
        count = read(input, block, BLOCK_SIZE),
        count > 0)
    {
        total_length += count;
        crc = crc32(crc, block, count);
    }
    if (count == -1)
        perror("Problem reading file");
    else
        printf("%s: %08x -- %d (%x) bytes\n",
            filename, crc, total_length, total_length);

    close(input);
}


int main(int argc, char **argv)
{
    if (argc < 2)
        printf("Usage: crc32 file [file...]\n");
    else
    {
        for (int i = 1; i < argc; i ++)
            print_crc32(argv[i]);
    }
    return 0;
}
