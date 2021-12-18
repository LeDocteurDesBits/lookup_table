#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "index.h"
#include "defines.h"

typedef struct {
    FILE* f;
    IndexHeader header;
} IndexFile;

void showProgress(uint64_t written, uint64_t total)
{
    float percents = (float) written / (float) total * 100;

    printf("\r\33[2K%lu / %lu (%.2f%%)\n", written, total, percents);
}

int openIndexFile(const char* filePath, IndexFile* out)
{
    out->f = fopen(filePath, "r");

    if(out->f == NULL)
    {
        return 1;
    }

    if(readIndexHeader(out->f, &out->header))
    {
        fclose(out->f);
        return 1;
    }

    return 0;
}

int copyWordlist(IndexFile* index, FILE* outputFile)
{
    uint8_t* copyBuffer = malloc(MIB);
    uint32_t readSize;
    int64_t currentIndexPos = ftell(index->f);

    if(copyBuffer == NULL)
    {
        return 1;
    }

    fseek(index->f, index->header.wordlistOffset + sizeof(IndexHeader), SEEK_SET);

    while((readSize = fread(copyBuffer, 1, MIB, index->f)) != 0)
    {
        fwrite(copyBuffer, readSize, 1, outputFile);
    }

    fseek(index->f, currentIndexPos, SEEK_SET);

    free(copyBuffer);
    return 0;
}

int main(int argc, char** argv)
{
    IndexFile indexFile1, indexFile2;
    FILE* outputFile;
    uint8_t indexEntrySize;
    uint64_t i, j, k, index1Count, index2Count, totalIndexCount, firstIndexWordlistSize, wordlistOffset;
    uint8_t* tmp1, *tmp2;

    if(argc != 4)
    {
        printf("Usage: %s <index_file1> <index_file2> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if(openIndexFile(argv[1], &indexFile1))
    {
        printf("Unable to open the first index file.\n");
        return EXIT_FAILURE;
    }

    if(openIndexFile(argv[2], &indexFile2))
    {
        printf("Unable to open the second index file.\n");

        fclose(indexFile1.f);
        return EXIT_FAILURE;
    }

    if(indexFile1.header.dataBytes != indexFile2.header.dataBytes)
    {
        printf("Index entry data bytes mismatch.\n");

        fclose(indexFile1.f);
        fclose(indexFile2.f);
        return EXIT_FAILURE;
    }

    if(memcmp(indexFile1.header.hashName, indexFile2.header.hashName, MAX_HASH_NAME_SIZE) != 0)
    {
        printf("Index hash names mismatch.\n");

        fclose(indexFile1.f);
        fclose(indexFile2.f);
        return EXIT_FAILURE;
    }

    outputFile = fopen(argv[3], "w");

    if(outputFile == NULL)
    {
        printf("Unable to open the output file.\n");

        fclose(indexFile1.f);
        fclose(indexFile2.f);
        return EXIT_FAILURE;
    }

    indexEntrySize = getIndexEntrySize(&indexFile1.header);
    index1Count = getIndexesCount(&indexFile1.header);
    index2Count = getIndexesCount(&indexFile2.header);
    totalIndexCount = index1Count + index2Count;

    firstIndexWordlistSize = getFileSize(indexFile1.f) - indexFile1.header.wordlistOffset - sizeof(IndexHeader);

    tmp1 = malloc(indexEntrySize);
    tmp2 = malloc(indexEntrySize);

    // This header is only a placeholder for now.
    writeIndexHeader(outputFile, indexFile1.header.hashName, indexFile1.header.dataBytes, 0);

    fread(tmp1, indexEntrySize, 1, indexFile1.f);
    fread(tmp2, indexEntrySize, 1, indexFile2.f);

    for(i=0, j=0, k=0 ; k<totalIndexCount ; k++)
    {
        if(i < index1Count && (j >= index2Count || (memcmp(tmp1, tmp2, INDEX_HASH_SIZE) < 0)))
        {
            fwrite(tmp1, indexEntrySize, 1, outputFile);
            fread(tmp1, indexEntrySize, 1, indexFile1.f);
            i++;
        }
        else
        {
            if(!(tmp2[indexEntrySize - 1] & INLINE_WORD_MASK))
            {
                writeIndexEntryPointer(tmp2,
                                       getPointerFromData(tmp2 + INDEX_HASH_SIZE, indexFile1.header.dataBytes) + firstIndexWordlistSize,
                                       indexFile1.header.dataBytes,
                                       (tmp2[indexEntrySize - 1] & WORD_TYPE_MASK) >> INLINE_WORD_BITS,
                                       outputFile);
            }
            else
            {
                fwrite(tmp2, indexEntrySize, 1, outputFile);
            }

            fread(tmp2, indexEntrySize, 1, indexFile2.f);
            j++;
        }

        if((k % PROGRESS_UPDATE_COUNT) == 0)
        {
            showProgress(k, totalIndexCount);
        }
    }

    wordlistOffset = ftell(outputFile) - sizeof(IndexHeader);

    copyWordlist(&indexFile1, outputFile);
    copyWordlist(&indexFile2, outputFile);

    rewind(outputFile);
    writeIndexHeader(outputFile, indexFile1.header.hashName, indexFile1.header.dataBytes, wordlistOffset);

    free(tmp1);
    free(tmp2);

    fclose(indexFile1.f);
    fclose(indexFile2.f);
    fclose(outputFile);

    return EXIT_SUCCESS;
}
