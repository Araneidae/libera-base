#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

int main()
{
    int msp = open("/dev/msp0", O_RDONLY);
    if (msp == -1)
    {
        perror("Open failed");
        return 1;
    }

    int32_t voltages[8];
    read(msp, voltages, sizeof(voltages));
    close(msp);

    int i;
    for (i = 0; i < 8; i ++)
        printf("    %.3f", 1e-3 * voltages[i]);
    printf("\n");
    return 0;
}
