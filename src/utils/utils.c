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
    if (fread(string, 1, fsize, f) != fsize)
    {
        printf("An error has occured while trying to read the content of the file!\n");
        free(string);
        return NULL;
    }
    fclose(f);

    string[fsize] = 0;
    
    return string;
}