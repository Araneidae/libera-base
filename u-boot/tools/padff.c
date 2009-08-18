/* Reads from stdin and pads to specified length with given character. */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MIN(x, y)   ((x) > (y) ? (y) : (x))

#define BLOCK_SIZE  4096

int main(int argc, char ** argv)
{
    if (argc == 3)
    {
        int pad_char      = strtol(argv[1], NULL, 0) & 0xff;
        int target_length = strtol(argv[2], NULL, 0);
        fprintf(stderr, "Padding stdin to %d bytes with 0x%02x\n",
            target_length, pad_char);

        int total_length = 0;
        char data_block[BLOCK_SIZE];
        int length_read;
        while (length_read = read(STDIN_FILENO, data_block, BLOCK_SIZE),
               length_read > 0)
        {
            total_length += length_read;
            int written = write(STDOUT_FILENO, data_block, length_read);
            if (written != length_read)
            {
                perror("Error writing output");
                return 1;
            }
        }

        memset(data_block, pad_char, BLOCK_SIZE);
        while (total_length < target_length)
        {
            int write_length = MIN(BLOCK_SIZE, target_length - total_length);
            int written = write(STDOUT_FILENO, data_block, write_length);
            if (written != write_length)
            {
                perror("Error writing output");
                return 1;
            }
            total_length += write_length;
        }
        return 0;
    }
    else
    {
        fprintf(stderr, "Usage: %s <padding> <length>\n", argv[0]);
        return 1;
    }
}
