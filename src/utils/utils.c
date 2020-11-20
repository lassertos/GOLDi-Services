#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

char* readFile(char* filename, unsigned int* filesize)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        printf("An error has occured while trying to open the file!\n");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (filesize != NULL)
    {
        *filesize = fsize;
    }

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

static const unsigned char base64Table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* encodeBase64(char* string, unsigned int lengthString)
{
    if (lengthString < 1)
    {
        return "";
    } 

    unsigned int lengthPaddedString = lengthString + 3 - (lengthString % 3);
    unsigned int lengthBase64String = (lengthPaddedString / 3) * 4;

    char* paddedString = malloc(lengthPaddedString + 1);
    memcpy(paddedString, string, lengthString);

    for (int i = lengthString; i <= lengthPaddedString; i++)
    {
        paddedString[i] = 0;
    }

    char* base64String = malloc(lengthBase64String + 1);
    base64String[lengthBase64String] = 0;

    for (int i = 0; i < lengthString; i = i+3)
    {
        //printf("input: %x %x %x\n", paddedString[i], paddedString[i+1], paddedString[i+2]);
        int j = (i / 3) * 4;

        unsigned int byte0 = (unsigned char)paddedString[i];
        unsigned int byte1 = (unsigned char)paddedString[i+1];
        unsigned int byte2 = (unsigned char)paddedString[i+2];

        unsigned int bytes = (byte0 << 16) + (byte1 << 8) + byte2;

        //printf("output: %x %x %x %x\n", base64String[j], base64String[j+1], base64String[j+2], base64String[j+3]);

        base64String[j] = base64Table[(bytes >> 18) & 0x3F];
        base64String[j+1] = base64Table[(bytes >> 12) & 0x3F];
        base64String[j+2] = base64Table[(bytes >> 6) & 0x3F];
        base64String[j+3] = base64Table[bytes & 0x3F];
    }

    for (int i = lengthBase64String - (lengthPaddedString - lengthString); i < lengthBase64String; i++)
    {
        base64String[i] = '=';
    }

    base64String[lengthBase64String] = '\0';
    
    free(paddedString);
    
    return base64String;
}

static char getBase64Value(char character)
{
    for (int i = 0; i < 64; i++)
    {
        if (character == base64Table[i])
        {
            return i;
        }
    }
    return -1;
}

char* decodeBase64(char* string, unsigned int* length)
{
    if (string != NULL)
    {
        if (strlen(string) < 1)
        {
            return "";
        } 
    }

    unsigned int lengthString = strlen(string);
    unsigned int equalsCount = 0;
    for (int i = 0; i < 3; i++)
    {
        if (string[lengthString-i] == '=')
        {
            equalsCount++;
        }
    }
    
    unsigned int lengthDecodedString = (lengthString / 4) * 3 - equalsCount;
    *length = lengthDecodedString;
    char* decodedString = malloc(lengthDecodedString+1);

    for (int i = 0; i < lengthString; i = i+4)
    {
        int j = (i / 4) * 3;

        decodedString[j] = getBase64Value(string[i]) << 2;
        decodedString[j] += getBase64Value(string[i+1]) >> 4;

        decodedString[j+1] = getBase64Value(string[i+1]) << 4;
        decodedString[j+1] += getBase64Value(string[i+2]) >> 2;

        decodedString[j+2] = getBase64Value(string[i+2]) << 6;
        decodedString[j+2] += getBase64Value(string[i+3]);
    }

    decodedString[lengthDecodedString] = 0;

    return decodedString;
}

char* serializeInt(int num)
{
    char* result = malloc(4);
    result[0] = num >> 24;
    result[1] = num >> 16;
    result[2] = num >> 8;
    result[3] = num;
    return result;
}

int deserializeInt(unsigned char* str)
{
    if (str == NULL)
        return -1;
    int result = str[0];
    result = (result << 8) + str[1];
    result = (result << 8) + str[2];
    result = (result << 8) + str[3];
    return result;
}