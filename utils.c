#include "utils.h"

uint64_t getFileSize(FILE* f)
{
    int64_t pos = ftell(f), size;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, pos, SEEK_SET);

    return size;
}

int isNumeric(char* s)
{
    for( ; *s ; s++)
    {
        if((*s < 0x30) || (*s > 0x39))
        {
            return 0;
        }
    }

    return 1;
}

int isAlphanumeric(char* s)
{
    for( ; *s ; s++)
    {
        if(!(((*s >= 0x30) && (*s <= 0x39)) || ((*s >= 0x41) && (*s <= 0x5A)) || ((*s >= 0x61) && (*s <= 0x7A))))
        {
            return 0;
        }
    }

    return 1;
}

int isReducedASCII(char* s)
{
    for( ; *s ; s++)
    {
        if(((uint8_t) *s) >= 0x7F)
        {
            return 0;
        }
    }

    return 1;
}

void compressNumeric(char* s, size_t n, uint8_t* out)
{
    uint32_t i;

    for(i=0 ; i<n ; i++, s++)
    {
        if(i & 1)
        {
            out[i >> 1] |= (*s - 0x30);
        }
        else
        {
            out[i >> 1] = (*s - 0x30) << 4;
        }
    }

    if(i & 1)
    {
        out[i >> 1] |= NUMERIC_STOP_SYMBOL;
    }
    else
    {
        out[i >> 1] = NUMERIC_STOP_SYMBOL << 4;
    }
}

void compressAlphanumeric(char* s, size_t n, uint8_t* out)
{
    uint32_t i, j;
    uint8_t b;

    for(i=0, j=0 ; i<n ; i++, j++, s++)
    {
        b = *s;

        if((b >= 0x30) && (b <= 0x39))
        {
            b -= 0x30;
        }
        else if((b >= 0x41) && (b <= 0x5A))
        {
            b -= 0x37;
        }
        else
        {
            b -= 0x3D;
        }

        switch(i % 4)
        {
            case 0:
            {
                out[j] = b << 2;
                break;
            }

            case 1:
            {
                out[j - 1] |= b >> 4;
                out[j] = b << 4;
                break;
            }

            case 2:
            {
                out[j - 1] |= b >> 2;
                out[j] = b << 6;
                break;
            }

            case 3:
            {
                j--;
                out[j] |= b;
                break;
            }
        }
    }

    switch(i % 4)
    {
        case 0:
        {
            out[j] = ALPHANUMERIC_STOP_SYMBOL << 2;
            break;
        }

        case 1:
        {
            out[j - 1] |= ALPHANUMERIC_STOP_SYMBOL >> 4;
            out[j] = (ALPHANUMERIC_STOP_SYMBOL << 4) & 0xFF;
            break;
        }

        case 2:
        {
            out[j - 1] |= ALPHANUMERIC_STOP_SYMBOL >> 2;
            out[j] = (ALPHANUMERIC_STOP_SYMBOL << 6) & 0xFF;
            break;
        }

        case 3:
        {
            out[j - 1] |= ALPHANUMERIC_STOP_SYMBOL;
            break;
        }
    }
}

void compressReducedASCII(char* s, size_t n, uint8_t* out)
{
    uint32_t i, j;
    uint8_t k, b;

    for(i=0, j=0 ; i<n ; i++, j++, s++)
    {
        k = i % 8;
        b = *s & 0x7F;

        if(k == 0)
        {
            out[j] = b << 1;
        }
        else if(k == 7)
        {
            j--;
            out[j] |= b;
        }
        else
        {
            out[j - 1] |= b >> (7 - k);
            out[j] = b << (k + 1);
        }
    }

    k = i % 8;

    if(k == 0)
    {
        out[j] = REDUCED_ASCII_STOP_SYMBOL << 1;
    }
    else if(k == 7)
    {
        out[j - 1] |= REDUCED_ASCII_STOP_SYMBOL;
    }
    else
    {
        out[j - 1] |= REDUCED_ASCII_STOP_SYMBOL >> (7 - k);
        out[j] = REDUCED_ASCII_STOP_SYMBOL << (k + 1);
    }
}

void uncompressNumeric(uint8_t* c, uint8_t* out)
{
    uint32_t i = 0;

    for( ; ; c++, i+=2)
    {
        if((*c >> 4) == NUMERIC_STOP_SYMBOL)
        {
            out[i] = 0x00;
            break;
        }

        out[i] = (*c >> 4) + 0x30;

        if((*c & 0xF) == NUMERIC_STOP_SYMBOL)
        {
            out[i + 1] = 0x00;
            break;
        }

        out[i + 1] = (*c & 0xF) + 0x30;
    }
}

void uncompressAlphanumeric(uint8_t* c, uint8_t* out)
{
    uint32_t i;
    uint8_t b;

    for(i=0 ; ; c++, i++)
    {
        switch(i % 4)
        {
            case 0:
            {
                b = *c >> 2;
                break;
            }

            case 1:
            {
                b = ((*(c - 1) << 4) | (*c >> 4)) & 0x3F;
                break;
            }

            case 2:
            {
                b = ((*(c - 1) << 2) | (*c >> 6)) & 0x3F;
                break;
            }

            case 3:
            {
                c--;
                b = *c & 0x3F;
                break;
            }
        }

        if(b == ALPHANUMERIC_STOP_SYMBOL)
        {
            out[i] = 0x00;
            break;
        }
        else if((b >= 0x00) && (b <= 0x09))
        {
            b += 0x30;
        }
        else if((b >= 0x0A) && (b <= 0x23))
        {
            b += 0x37;
        }
        else
        {
            b += 0x3D;
        }

        out[i] = b;
    }
}

void uncompressReducedASCII(uint8_t* c, uint8_t* out)
{
    uint32_t i;
    uint8_t j;

    for(i=0 ; ; c++, i++)
    {
        j = i % 8;

        if(j == 0)
        {
            out[i] = *c >> 1;
        }
        else if(j == 7)
        {
            c--;
            out[i] = *c & 0x7F;
        }
        else
        {
            out[i] = ((*(c - 1) << (7 - j)) | (*c >> (j + 1))) & 0x7F;
        }

        if(out[i] == REDUCED_ASCII_STOP_SYMBOL)
        {
            out[i] = 0x00;
            break;
        }
    }
}

void unhex(char* hex, uint8_t* out, size_t n)
{
    static const uint8_t unhexTable[256] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    size_t i;
    char c1, c2;

    for(i=0 ; i<n ; i++)
    {
        c1 = *hex++;
        c2 = *hex++;

        out[i] = (unhexTable[c1] << 4) | unhexTable[c2];
    }
}