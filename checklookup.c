#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"
#include "index.h"
#include "hash.h"
#include "defines.h"

void readWord(uint8_t* indexData, uint8_t* wordlist, uint8_t indexDataSize, uint8_t* out)
{
    uint8_t* word = indexData;
    uint8_t lastByte = indexData[indexDataSize - 1];

    if(!(lastByte & INLINE_WORD_MASK))
    {
        word = wordlist + getPointerFromData(indexData, indexDataSize);
    }

    switch((lastByte & WORD_TYPE_MASK) >> INLINE_WORD_BITS)
    {
        case NUMERIC:
            uncompressNumeric(word, out);
            break;

        case ALPHANUMERIC:
            uncompressAlphanumeric(word, out);
            break;

        case REDUCED_ASCII:
            uncompressReducedASCII(word, out);
            break;

        default:
            strcpy((char*) out, (char*) word);
            break;
    }
}

void lookup(uint8_t* index, uint8_t* wordlist, uint64_t indexesCount, uint8_t indexEntrySize, uint8_t indexDataSize,
            HashInfos* hashInfos, uint8_t* digestTmp, uint8_t* hash, uint8_t* out)
{
    uint64_t l = 0, u = indexesCount - 1, m;
    int cmp;

    *out = '\0';

    while(u >= l)
    {
        m = l + (u - l) / 2;
        cmp = memcmp(index + m * indexEntrySize, hash, INDEX_HASH_SIZE);

        if(cmp > 0)
        {
            u = m - 1;
        }
        else if(cmp < 0)
        {
            l = m + 1;
        }
        else
        {
            while((m != 0xFFFFFFFFFFFFFFFFL) && (memcmp(index + m * indexEntrySize, hash, INDEX_HASH_SIZE) == 0))
            {
                m--;
            }

            m++;

            while((m < indexesCount) && (memcmp(index + m * indexEntrySize, hash, INDEX_HASH_SIZE) == 0))
            {
                readWord(index + m * indexEntrySize + INDEX_HASH_SIZE, wordlist, indexDataSize, out);
                hashInfos->f(out, strlen((char*) out), digestTmp);

                if(memcmp(hash, digestTmp, hashInfos->digestSize) == 0)
                {
                    return;
                }

                *out = '\0'; // Put a 0 again on the first output byte to know when a result is found or not.
                m++;
            }

            return;
        }
    }
}

void showProgress(uint64_t goodAnswers, uint64_t totalAnswers, uint64_t nullBytesPasswords)
{
    uint64_t badAnswers = totalAnswers - goodAnswers - nullBytesPasswords;
    float goodPercents = (float) goodAnswers / (float) totalAnswers * 100;

    printf("\033[A\r\33[2K%lu good / %lu bad / %lu null, %lu (%.2f%%)\n", goodAnswers, badAnswers,
           nullBytesPasswords, totalAnswers, goodPercents);
}

int main(int argc, char** argv)
{
    uint8_t answer, indexEntrySize, indexDataSize;
    uint64_t bufSize, indexesCount;
    FILE* indexFile, *wordlistFile;
    IndexHeader indexHeader;
    HashInfos hashInfos;
    uint8_t* index = NULL, *wordlist = NULL, *digest = NULL, *digestTmp = NULL;
    uint8_t lookupResult[MAX_LINE_SIZE];
    char line[MAX_LINE_SIZE] = {0};
    char* tmp;
    size_t lineLength;
    uint64_t goodAnswers = 0, totalAnswers = 0, nullBytesPasswords = 0;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    if(argc != 3)
    {
        printf("Usage: %s <index_file> <wordlist_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    wordlistFile = fopen(argv[2], "r");

    if(wordlistFile == NULL)
    {
        printf("Unable to open the wordlist file.\n");
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

    indexEntrySize = getIndexEntrySize(&indexHeader);
    indexDataSize = indexHeader.dataBytes;
    indexesCount = getIndexesCount(&indexHeader);

    getHashInfos(indexHeader.hashName, &hashInfos);

    if(hashInfos.f == NULL)
    {
        printf("Unable to find the hash function named: %s\n", indexHeader.hashName);

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    digest = malloc(hashInfos.digestSize);
    digestTmp = malloc(hashInfos.digestSize);

    fseek(indexFile, 0, SEEK_END);
    bufSize = ftell(indexFile) - sizeof(IndexHeader);
    fseek(indexFile, sizeof(IndexHeader), SEEK_SET);

    printf("WARNING: This program will allocate %lu MiB of RAM. Do you want to continue? (y/N)\n", bufSize / MIB);
    answer = getchar();

    if((answer != 'y') && (answer != 'Y'))
    {
        printf("ABORTING\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    index = malloc(bufSize);

    if(index == NULL)
    {
        printf("Unable to allocate the index.\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    wordlist = index + indexHeader.wordlistOffset;
    fread(index, sizeof(uint8_t), bufSize, indexFile);

    fclose(indexFile);

    printf("The index is loaded successfully.\n");

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

        if(strlen(line) != lineLength)
        {
            nullBytesPasswords++;
        }
        else
        {
            hashInfos.f((uint8_t*) line, lineLength, digest);
            lookup(index, wordlist, indexesCount, indexEntrySize, indexDataSize, &hashInfos, digestTmp, digest, lookupResult);

            if(memcmp(line, lookupResult, lineLength) == 0)
            {
                goodAnswers++;
            }
            else
            {
                printf("ERROR: %s\n", line);
            }
        }

        memset(line, '\0', MAX_LINE_SIZE);

        totalAnswers++;

        if((totalAnswers % PROGRESS_UPDATE_COUNT) == 0)
        {
            showProgress(goodAnswers, totalAnswers, nullBytesPasswords);
        }
    }

    showProgress(goodAnswers, totalAnswers, nullBytesPasswords);

    free(index);
    free(digest);
    free(digestTmp);

    return EXIT_SUCCESS;
}
