#include <stdio.h>


/* This simply implements the following bash script:
 *
 *  tail -c+5 | tr '\0' '\n' | sed '/^$/q'
 *
 * Unfortunately we don't have tr on Libera 1.46 and I can't persuade sed to
 * do the job either. */

int main()
{
    /* Simple simple simple. */
    for (int i = 0; i < 4; i ++)
        fgetc(stdin);

    int eos = 1;
    while (1)
    {
        int ch = fgetc(stdin);
        if (ch == EOF)
            return 0;
        if (ch == '\0')
        {
            if (eos)
                return 0;
            fputc('\n', stdout);
            eos = 1;
        }
        else
        {
            eos = 0;
            fputc(ch, stdout);
        }
    }
}
