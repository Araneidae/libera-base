/* Generate configuration block from stdin adn write to stdout, called ast
 *
 *  generate-block | makeconfig $block_size [$env_size]
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE 1024       // Random limit


extern unsigned long crc32 (
    unsigned long, const unsigned char *, unsigned int);


static inline void Fail(const char *message, ...)
{
    if (errno)
        perror("ooops");
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    exit(1);
}


int ReadHex(const char *arg)
{
    char * end;
    int result = strtol(arg, &end, 16);
    if (end == arg  ||  *end != '\0')
        Fail("Invalid argument: \"%s\" is not a hex number\n", arg);
    return result;
}


void LoadBlock(unsigned char * EnvPtr, int FreeSpace)
{
    char Line[MAX_LINE];
    while (fgets(Line, MAX_LINE, stdin))
    {
        int length = strlen(Line);
        if (Line[length-1] == '\n')
            Line[length-1] = '\0';
        else if (length == MAX_LINE - 1)
            Fail("Line starting \"%.40s...\" too long\n", Line);
        else
            length += 1;

        if (strchr(Line, '=') == NULL)
            Fail("No = in line \"%s\"\n", Line);

        if (FreeSpace < length + 1)
            Fail("No room left in environment\n");

        memcpy(EnvPtr, Line, length);
        EnvPtr += length;
    }
    *EnvPtr = '\0';
}


void WriteBlock(const unsigned char * block, int length)
{
    while (length > 0)
    {
        int written = write(1, block, length);
        if (written == -1)
            Fail("Problem writing output\n");
        block += written;
        length -= written;
    }
}


int main(int argc, char **argv)
{
    if (argc < 2  ||  argc > 3)
        Fail("Usage: makeconfig $block_size [$env_size] <params >block\n");

    int BlockSize = ReadHex(argv[1]);
    int EnvSize = argc > 2 ? ReadHex(argv[2]) : BlockSize;
    if (EnvSize > BlockSize)
        Fail("Environment doesn't fit in block!\n");

    unsigned char Block[BlockSize];
    memset(Block, 0xff, BlockSize);
    memset(Block, 0, EnvSize);

    LoadBlock(Block + 4, EnvSize - 4);
    *(unsigned long*) Block = crc32(0, Block + 4, EnvSize - 4);
    WriteBlock(Block, BlockSize);
    return 0;
}
