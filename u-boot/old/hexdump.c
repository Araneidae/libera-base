#include <stdio.h>
#include <unistd.h>
#include <string.h>


static char safe_char(unsigned char value)
{
    return (' ' <= value  &&  value < 127) ? value : '.';
}


int main()
{
    unsigned char line[16];
    int * words = (int *) line;

    int offset = 0;
    int count;
    while (
        memset(line, 0, 16),
        count = read(0, line, 16),
        count > 0)
    {
        printf("%08x: ", offset);
        offset += count;

        for (int i = 0; i < 4; i++)
        {
            if (4*i < count)
                printf("%08x ", words[i]);
            else
                printf("         ");
        }
        printf("   ");
        for (int i = 0; i < count; i++)
            printf("%c", safe_char(line[i]));
        printf("\n");
    }
    return 0;
}
