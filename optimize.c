#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "utils.h"
#include "index.h"
#include "defines.h"

typedef struct {
    uint64_t inlineCount;
    uint64_t pointerCount;
    uint64_t inlineTypes[WORD_TYPE_COUNT];
    uint64_t pointerTypes[WORD_TYPE_COUNT];
    uint64_t size;
} IndexStats;

int main(int argc, char** argv)
{
    FILE* wordlistFile = NULL;
    char line[MAX_LINE_SIZE] = {0};
    char* tmp;
    uint8_t minDataBits, minIndex = 0;
    uint32_t i;
    uint64_t minIndexSize = -1;
    size_t lineLength, compressedBits;
    WordType wordType;
    IndexStats* stats = NULL;

    if(argc != 2)
    {
        printf("Usage: %s <wordlist>\n", argv[0]);
        return EXIT_FAILURE;
    }

    wordlistFile = fopen(argv[1], "r");

    if(wordlistFile == NULL)
    {
        printf("Unable to open the wordlist file.\n");
        return EXIT_FAILURE;
    }

    minDataBits = getMinDataBits(wordlistFile);
    stats = malloc((MAX_DATA_BITS - minDataBits + 1) * sizeof(IndexStats));

    if(stats == NULL)
    {
        printf("Error: Unable to allocate the stats array.\n");

        fclose(wordlistFile);
        return EXIT_FAILURE;
    }

    memset(stats, 0x00, (MAX_DATA_BITS - minDataBits + 1) * sizeof(IndexStats));

    while(fgets(line, MAX_LINE_SIZE, wordlistFile) != NULL)
    {
        tmp = memchr(line, '\r', MAX_LINE_SIZE);

        if(tmp == NULL)
        {
            tmp = memchr(line, '\n', MAX_LINE_SIZE);
        }

        if(tmp == NULL)
        {
            printf("Error: the line is too long (larger than %u characters).\n", MAX_LINE_SIZE - 1);

            fclose(wordlistFile);
            return EXIT_FAILURE;
        }

        *tmp = '\0';
        lineLength = tmp - line;

        if(isNumeric(line))
        {
            compressedBits = NUMERIC_COMPRESSED_BITS(lineLength) + NUMERIC_SYMBOL_BITS;
            wordType = NUMERIC;
        }
        else if(isAlphanumeric(line))
        {
            compressedBits = ALPHANUMERIC_COMPRESSED_BITS(lineLength) + ALPHANUMERIC_SYMBOL_BITS;
            wordType = ALPHANUMERIC;
        }
        else if(isReducedASCII(line))
        {
            compressedBits = REDUCED_ASCII_COMPRESSED_BITS(lineLength) + REDUCED_ASCII_SYMBOL_BITS;
            wordType = REDUCED_ASCII;
        }
        else
        {
            compressedBits = (lineLength + 1) << 3;
            wordType = NO_COMPRESSION;
        }

        for(i=0 ; i<=MAX_DATA_BITS - minDataBits ; i++)
        {
            if(compressedBits + 3 <= minDataBits + i)
            {
                stats[i].inlineCount++;
                stats[i].inlineTypes[wordType]++;
                stats[i].size += INDEX_HASH_SIZE + BYTES_SIZE(minDataBits + i);
            }
            else
            {
                stats[i].pointerCount++;
                stats[i].pointerTypes[wordType]++;
                stats[i].size += INDEX_HASH_SIZE + BYTES_SIZE(minDataBits + i) + BYTES_SIZE(compressedBits);
            }
        }
    }

    for(i=0 ; i<=MAX_DATA_BITS - minDataBits ; i++)
    {
        if(stats[i].size <= minIndexSize)
        {
            minIndexSize = stats[i].size;
            minIndex = i;
        }
    }

    printf("=========== %u bits data ===========\n", minDataBits + minIndex);
    printf("+ inlines: %lu (%.02f%%)\n", stats[minIndex].inlineCount, 100.0 * (double) stats[minIndex].inlineCount / (double) (stats[minIndex].inlineCount + stats[minIndex].pointerCount));
    printf("\t+ no compression: %lu\n", stats[minIndex].inlineTypes[NO_COMPRESSION]);
    printf("\t+ numeric: %lu\n", stats[minIndex].inlineTypes[NUMERIC]);
    printf("\t+ alphanumeric: %lu\n", stats[minIndex].inlineTypes[ALPHANUMERIC]);
    printf("\t+ reduced ASCII: %lu\n", stats[minIndex].inlineTypes[REDUCED_ASCII]);
    printf("+ pointers: %lu\n", stats[minIndex].pointerCount);
    printf("\t+ no compression: %lu\n", stats[minIndex].pointerTypes[NO_COMPRESSION]);
    printf("\t+ numeric: %lu\n", stats[minIndex].pointerTypes[NUMERIC]);
    printf("\t+ alphanumeric: %lu\n", stats[minIndex].pointerTypes[ALPHANUMERIC]);
    printf("\t+ reduced ASCII: %lu\n", stats[minIndex].pointerTypes[REDUCED_ASCII]);
    printf("+ size (in bytes): %lu\n", stats[minIndex].size + sizeof(IndexHeader));
    printf("====================================\n");

    fclose(wordlistFile);

    return EXIT_SUCCESS;
}