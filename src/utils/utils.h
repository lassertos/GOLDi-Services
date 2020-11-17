#ifndef UTILS_H
#define UTILS_H

char* readFile(char* filename, unsigned int* filesize);
char* encodeBase64(char* string, unsigned int lengthString);
char* decodeBase64(char* string, unsigned int* length);
char* serializeInt(int num);
int deserializeInt(unsigned char* str);

#endif