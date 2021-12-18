#ifndef HASH_H
#define HASH_H

#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

typedef struct {
    unsigned char* (*f)(const unsigned char*, size_t, unsigned char*);
    unsigned char digestSize;
} HashInfos;

void getHashInfos(const char* hashName, HashInfos* out);

#endif //HASH_H
