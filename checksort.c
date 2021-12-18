#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "index.h"
#include "defines.h"


void showProgress(uint64_t entry, uint64_t entryCount)
{
    float percents = (float) entry / (float) entryCount * 100;

    printf("\033[A\r\33[2K%lu / %lu (%.2f%%)\n", entry, entryCount, percents);
}

int main(int argc, char **argv)
{
    FILE* indexFile = NULL;
    uint64_t entriesCount, i;
    uint8_t* currentEntry = NULL, *nextEntry = NULL;
    IndexHeader indexHeader;
    uint8_t indexEntrySize;

    if(argc != 2)
    {
        printf("Usage: %s <index_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    indexFile = fopen(argv[1], "r");

    if(indexFile == NULL)
    {
        printf("Unable to open the index file.\n");
        return EXIT_FAILURE;
    }

    if(readIndexHeader(indexFile, &indexHeader))
    {
        printf("Invalid index file.\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    entriesCount = getIndexesCount(&indexHeader);

    if(entriesCount < 2)
    {
        printf("This index must be sorted...\n");

        fclose(indexFile);
        return EXIT_SUCCESS;
    }

    indexEntrySize = getIndexEntrySize(&indexHeader);
    currentEntry = malloc(indexEntrySize);
    nextEntry = malloc(indexEntrySize);

    fread(currentEntry, indexEntrySize, 1, indexFile);

    for(i=0 ; i<(entriesCount-1) ; i++)
    {
        fread(nextEntry, indexEntrySize, 1, indexFile);

        if(memcmp(currentEntry, nextEntry, INDEX_HASH_SIZE) > 0)
        {
            printf("The index is not sorted!\n");

            free(currentEntry);
            free(nextEntry);

            fclose(indexFile);
            return EXIT_FAILURE;
        }

        memcpy(currentEntry, nextEntry, INDEX_HASH_SIZE);

        if((i % PROGRESS_UPDATE_COUNT) == 0)
        {
            showProgress(i, entriesCount);
        }
    }

    printf("The index is sorted!\n");

    free(currentEntry);
    free(nextEntry);

    fclose(indexFile);

    return EXIT_SUCCESS;
}
