#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define ENV_SIZE 0x4000



int main()
{
// 
//     int mtd1 = open("/dev/mtdblock1", O_RDONLY);
//     if (mtd1 == -1)
//     {
//         printf("Can't open environment file\n");
//         return 1;
//     }
// 
    char env[ENV_SIZE];
    memset(env, 0, ENV_SIZE);
//     read(mtd1, env, ENV_SIZE);
    read(0, env, ENV_SIZE);
//    close(mtd1);
    for (char * string = env + 4; *string != '\0';
         string += strlen(string) + 1)
        printf("%s\n", string);
    return 0;
}
