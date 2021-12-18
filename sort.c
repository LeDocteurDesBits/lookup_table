#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "index.h"
#include "defines.h"

void mergeSort(uint8_t* sortBuffer, uint8_t* workBuffer, uint64_t l, uint64_t u, uint8_t indexEntrySize);
void merge(const uint8_t* in, uint8_t* out, uint64_t l, uint64_t m, uint64_t u, uint8_t indexEntrySize);

void loadFileToBuffer(FILE* file, uint8_t* buffer, uint64_t indexesCount, uint8_t indexEntrySize);
void writeBufferToFile(FILE* file, uint8_t* buffer, uint64_t indexesCount, uint8_t indexEntrySize);

int main(int argc, char** argv)
{
    FILE* indexFile;
    uint64_t bufSize, indexesCount;
    uint8_t indexEntrySize, answer;
    uint8_t* sortBuffer, *workBuffer;
    IndexHeader indexHeader;

    if(argc != 2)
    {
        printf("Usage: %s <index_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    indexFile = fopen(argv[1], "r+");

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

    indexEntrySize = getIndexEntrySize(&indexHeader);
    indexesCount = getIndexesCount(&indexHeader);
    bufSize = indexEntrySize * indexesCount;

    printf("WARNING: This program will allocate %lu MiB of RAM. Do you want to continue? (y/N)\n", (2 * bufSize) / MIB);
    answer = getchar();

    if((answer != 'y') && (answer != 'Y'))
    {
        printf("ABORTING\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    sortBuffer = malloc(bufSize);

    if(sortBuffer == NULL)
    {
        printf("Unable to allocate the sort buffer.\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    workBuffer = malloc(bufSize);

    if(workBuffer == NULL)
    {
        printf("Unable to allocate the merge buffer.\n");

        free(sortBuffer);
        fclose(indexFile);
        return EXIT_FAILURE;
    }

    loadFileToBuffer(indexFile, sortBuffer, indexesCount, indexEntrySize);
    memcpy(workBuffer, sortBuffer, bufSize);

    mergeSort(sortBuffer, workBuffer, 0, indexesCount, indexEntrySize);

    writeBufferToFile(indexFile, sortBuffer, indexesCount, indexEntrySize);

    free(sortBuffer);
    free(workBuffer);
    fclose(indexFile);

    return EXIT_SUCCESS;
}

void mergeSort(uint8_t* sortBuffer, uint8_t* workBuffer, uint64_t l, uint64_t u, uint8_t indexEntrySize)
{
    uint64_t m;

    if(u - l <= 1)
    {
        return;
    }

    m = l + (u - l) / 2;

    mergeSort(workBuffer, sortBuffer, l, m, indexEntrySize);
    mergeSort(workBuffer, sortBuffer, m, u, indexEntrySize);

    merge(workBuffer, sortBuffer, l, m, u, indexEntrySize);
}

void merge(const uint8_t* in, uint8_t* out, uint64_t l, uint64_t m, uint64_t u, uint8_t indexEntrySize)
{
    uint64_t i = l, j = m, k;

    for(k=l ; k<u ; k++)
    {
        if(i < m && (j >= u || (memcmp(in + i * indexEntrySize, in + j * indexEntrySize, INDEX_HASH_SIZE) < 0)))
        {
            memcpy(out + k * indexEntrySize, in + i * indexEntrySize, indexEntrySize);
            i++;
        }
        else
        {
            memcpy(out + k * indexEntrySize, in + j * indexEntrySize, indexEntrySize);
            j++;
        }
    }
}

void loadFileToBuffer(FILE* file, uint8_t* buffer, uint64_t indexesCount, uint8_t indexEntrySize)
{
    fseek(file, sizeof(IndexHeader), SEEK_SET);
    fread(buffer, indexEntrySize, indexesCount, file);
}

void writeBufferToFile(FILE* file, uint8_t* buffer, uint64_t indexesCount, uint8_t indexEntrySize)
{
    fseek(file, sizeof(IndexHeader), SEEK_SET);
    fwrite(buffer, indexEntrySize, indexesCount, file);
}