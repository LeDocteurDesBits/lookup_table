#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "index.h"
#include "hash.h"
#include "defines.h"

void showProgress(uint64_t offset, uint64_t maxOffset, uint64_t hashesGenerated)
{
    float percents = (float) offset / (float) maxOffset * 100;

    printf("\033[A\r\33[2K%lu / %lu (%.2f%%) - %lu hashes generated\n", offset, maxOffset, percents, hashesGenerated);
}

int main(int argc, char** argv)
{
    HashInfos hashInfos;
    FILE* wordlistFile = NULL, *outputFile = NULL, *tmpFile = NULL;
    uint8_t* digest = NULL;
    uint8_t* copyBuffer = malloc(MIB);
    char line[MAX_LINE_SIZE] = {0};
    uint8_t compressedLine[MAX_LINE_SIZE] = {0};
    char* tmp;
    size_t lineLength, indexDataBits, indexDataBytes;
    uint64_t wordlistFileSize, offset, i = 0;
    uint32_t readSize;
    uint64_t wordlistOffset;
    WordType wordType;
    uint32_t compressedBitsSize;

    hashInfos.f = NULL;

    if(argc != 6)
    {
        printf("Usage: %s <hash_function> <index_data_bits> <wordlist_file> <output_file> <tmp_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    getHashInfos(argv[1], &hashInfos);

    if(hashInfos.f == NULL)
    {
        printf("Hash name %s is not recognized. Supported hashes are: md5, sha1, sha256.\n", argv[1]);
        return EXIT_FAILURE;
    }

    wordlistFile = fopen(argv[3], "r");

    if(wordlistFile == NULL)
    {
        printf("Unable to open the wordlist file.\n");
        return EXIT_FAILURE;
    }

    outputFile = fopen(argv[4], "w");

    if(outputFile == NULL)
    {
        printf("Unable to open the output file.\n");
        return EXIT_FAILURE;
    }

    tmpFile = fopen(argv[5], "w+");

    if(tmpFile == NULL)
    {
        printf("Unable to open the temporary file.\n");
        return EXIT_FAILURE;
    }

    wordlistFileSize = getFileSize(wordlistFile);
    indexDataBits = strtol(argv[2], NULL, 10);
    indexDataBytes = BYTES_SIZE(indexDataBits);

    if(!isDataSizeValid(wordlistFile, indexDataBits))
    {
        printf("Invalid data size.\n");
        return EXIT_FAILURE;
    }

    digest = malloc(hashInfos.digestSize);

    // This header is only a placeholder for now
    writeIndexHeader(outputFile, argv[1], indexDataBytes, 0);

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
            return EXIT_FAILURE;
        }

        *tmp = '\0';
        lineLength = tmp - line;

        hashInfos.f((uint8_t*) line, lineLength, digest);

        if(isNumeric(line))
        {
            compressNumeric(line, lineLength, compressedLine);
            wordType = NUMERIC;
            compressedBitsSize = NUMERIC_COMPRESSED_BITS(lineLength) + NUMERIC_SYMBOL_BITS;
        }
        else if(isAlphanumeric(line))
        {
            compressAlphanumeric(line, lineLength, compressedLine);
            wordType = ALPHANUMERIC;
            compressedBitsSize = ALPHANUMERIC_COMPRESSED_BITS(lineLength) + ALPHANUMERIC_SYMBOL_BITS;
        }
        else if(isReducedASCII(line))
        {
            compressReducedASCII(line, lineLength, compressedLine);
            wordType = REDUCED_ASCII;
            compressedBitsSize = REDUCED_ASCII_COMPRESSED_BITS(lineLength) + REDUCED_ASCII_SYMBOL_BITS;
        }
        else
        {
            wordType = NO_COMPRESSION;
            compressedBitsSize = (lineLength + 1) << 3;
        }

        if(compressedBitsSize + 3 <= indexDataBits)
        {
            if(wordType == NO_COMPRESSION)
            {
                writeIndexEntryInline(digest, (uint8_t*) line, compressedBitsSize, indexDataBytes, NO_COMPRESSION, outputFile);
            }
            else
            {
                writeIndexEntryInline(digest, compressedLine, compressedBitsSize, indexDataBytes, wordType, outputFile);
            }
        }
        else
        {
            writeIndexEntryPointer(digest, ftell(tmpFile), indexDataBytes, wordType, outputFile);

            if(wordType == NO_COMPRESSION)
            {
                fwrite(line, sizeof(char), BYTES_SIZE(compressedBitsSize), tmpFile);
            }
            else
            {
                fwrite(compressedLine, sizeof(uint8_t), BYTES_SIZE(compressedBitsSize), tmpFile);
            }
        }

        memset(line, '\0', MAX_LINE_SIZE);

        offset = ftell(wordlistFile);
        i++;

        if((i % PROGRESS_UPDATE_COUNT) == 0)
        {
            showProgress(offset, wordlistFileSize, i);
        }
    }

    wordlistOffset = ftell(outputFile) - sizeof(IndexHeader);
    rewind(tmpFile);

    while((readSize = fread(copyBuffer, 1, MIB, tmpFile)) != 0)
    {
        fwrite(copyBuffer, readSize, 1, outputFile);
    }

    rewind(outputFile);
    writeIndexHeader(outputFile, argv[1], indexDataBytes, wordlistOffset);

    free(copyBuffer);
    free(digest);

    fclose(wordlistFile);
    fclose(outputFile);
    fclose(tmpFile);

    unlink(argv[5]);

    return EXIT_SUCCESS;
}
