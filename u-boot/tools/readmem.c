/* Simple tool to read a block of memory.  A little like dd, but it works
 * on /dev/mem (which dd unfortunately doesn't). */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>



int DumpMemory(int mem, unsigned int start, unsigned int length)
{
    int OsPageSize = getpagesize();
    int OsPageMask = OsPageSize - 1;

    unsigned int end = start + length - 1;
    unsigned int first_page = start & ~OsPageMask;
    unsigned int last_page  = end   & ~OsPageMask;
    unsigned int offset = start & OsPageMask;
    for (unsigned int page = first_page; page <= last_page; page += OsPageSize)
    {
        void * mapped_page = mmap(
            0, OsPageSize, PROT_READ, MAP_SHARED, mem, page);
        if (mapped_page == MAP_FAILED)
        {
            perror("Unable to map address");
            return 2;
        }

        size_t ToWrite = page == last_page ?
            (end & OsPageMask) + 1 - offset :
            OsPageSize - offset;
        ssize_t written = write(
            STDOUT_FILENO, mapped_page + offset, ToWrite);
        offset = 0;
        
        if (written != (ssize_t) ToWrite)
        {
            perror("Unable to write out page");
            return 2;
        }

        if (munmap(mapped_page, OsPageSize) != 0)
        {
            perror("Error unmapping memory");
            return 2;
        }
    }
    return 0;
}


int main(int argc, char **argv)
{
    if (argc == 3)
    {
        unsigned int start  = strtol(argv[1], NULL, 0);
        unsigned int length = strtol(argv[2], NULL, 0);
        if ((start & 3) != 0  ||  (length & 3) != 0)
        {
            fprintf(stderr, "Word boundaries only: %08x, %08x\n",
                start, length);
            return 1;
        }
        if (start + length < start)     // Unsigned overflow
        {
            fprintf(stderr, "Block too long to read: %08x, %08x\n",
                start, length);
            return 1;
        }

        int mem = open("/dev/mem", O_RDONLY);
        if (mem < 0)
        {
            perror("Unable to open /dev/mem");
            return 2;
        }
        return DumpMemory(mem, start, length);
    }
    else
    {
        fprintf(stderr, "Usage: %s <start> <length>\n", argv[0]);
        return 1;
    }
}
