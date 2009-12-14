#include <stdio.h>
#include <stdbool.h>


/* This simply implements the following bash script:
 *
 *  tail -c+5 | tr '\0' '\n' | sed '/^$/q'
 *
 * Unfortunately we don't have tr on Libera 1.46 and I can't persuade sed to
 * do the job either.
 *
 * If an argument is given then the behaviour is slightly different, nearly
 * equivalent to the following script, which prints the value associated with
 * a variable -- except, this program allows the value to contain newlines.
 *
 *  tail -c+5 | tr '\0' '\n' | sed -n '/^'"$1"'=/{s///;p;q;}'
 */

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [variable]\n", argv[0]);
        return 2;
    }
    
    /* Skip the checksum. */
    for (int i = 0; i < 4; i ++)
        fgetc(stdin);

    bool searching = argc == 2; // Set if looking for a match
    bool printing = !searching; // Set if output is to be printed
    char *match = argv[1];      // Keyword to search for
    bool matched = searching;   // True if match succeeded so far
    bool eos = true;            // End of string marker
    while (true)
    {
        int ch = fgetc(stdin);
        if (ch == EOF)
            return printing ? 0 : 1;
        if (ch == '\0')
        {
            if (searching  &&  printing)
                return 0;       // End of a successful match
            if (eos)
                return printing ? 0 : 1;
            if (printing)
                fputc('\n', stdout);
            
            eos = true;
            matched = searching; // Enable search for match again
            match = argv[1];
        }
        else
        {
            eos = false;
            if (printing)
                fputc(ch, stdout);
            
            if (matched)
            {
                if (*match == '\0')
                {
                    printing = ch == '=';
                    matched = false;    // Stop searching after success
                }
                else
                    matched = ch == *match++; 
            }
        }
    }
}
