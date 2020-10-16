#include "utils.h"

char* readFile(char* filename)
{
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    if (string == NULL)
    {
        return NULL;
    }
    fread(string, 1, fsize, f);
    fclose(f);

    string[fsize] = 0;
    
    return string;
}