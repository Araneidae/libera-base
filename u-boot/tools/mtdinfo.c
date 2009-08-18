/* Simple MTD info tool. */

#include <stdio.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mtd/mtd-user.h>
//#include <mtd/mtd.h>

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        for (int i = 1; i < argc; i++)
        {
            char * arg = argv[i];
            int mtd = open(arg, O_RDONLY);
            if (mtd < 0)
            {
                perror("Unable to open device");
                return 1;
            }

            struct mtd_info_user mtd_info;
            if (ioctl(mtd, MEMGETINFO, &mtd_info) < 0)
            {
                perror("Unable to read mtd info");
                return 1;
            }

            printf("%s: %d %08x %08x %08x %08x %08x\n",
                arg, mtd_info.type, mtd_info.flags, mtd_info.size,
                mtd_info.erasesize, mtd_info.writesize, mtd_info.oobsize);

            const char * types[] = {
                "Absent",       "RAM",          "ROM",          "NORFLASH",
                "NANDFLASH",    "(unknown)",    "DATAFLASH",    "UBIVOLUME"};
            printf("%s type: ", arg);
            if (mtd_info.type < 8)
                printf(types[mtd_info.type]);
            else
                printf("(unknown)");
            printf("\n");
            
            printf("%s flags:", arg);
            if (mtd_info.flags & MTD_WRITEABLE)
                printf(" writeable");
            if (mtd_info.flags & MTD_BIT_WRITEABLE)
                printf(" bit-writeable");
            if (mtd_info.flags & MTD_NO_ERASE)
                printf(" no-erase");
            if (mtd_info.flags & MTD_POWERUP_LOCK)
                printf(" powerup-lock");
            printf("\n");

            printf(
                "%s size = 0x%08x, erasesize = 0x%08x, writesize = 0x%08x\n",
                arg, mtd_info.size, mtd_info.erasesize, mtd_info.writesize);

            close(mtd);
        }

        return 0;
    }
    else
    {
        printf("Usage: %s <device>\n", argv[0]);
        return 1;
    }
}
