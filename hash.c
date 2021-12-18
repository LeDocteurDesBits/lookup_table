#include "hash.h"

void getHashInfos(const char* hashName, HashInfos* out)
{
    if(strncmp(hashName, "md5", 3) == 0)
    {
        out->f = MD5;
        out->digestSize = MD5_DIGEST_LENGTH;
    }

    if(strncmp(hashName, "sha1", 3) == 0)
    {
        out->f = SHA1;
        out->digestSize = SHA_DIGEST_LENGTH;
    }

    if(strncmp(hashName, "sha256", 6) == 0)
    {
        out->f = SHA256;
        out->digestSize = SHA256_DIGEST_LENGTH;
    }
}