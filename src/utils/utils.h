#ifndef UTILS_H
#define UTILS_H

char* readFile(char* filename);
char* encodeBase64(unsigned char* string);
char* decodeBase64(char* string);
char* serializeInt(int num);
int deserializeInt(unsigned char* str);

#endif