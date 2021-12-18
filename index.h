#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "utils.h"

#define INDEX_MAGIC 0x3A1DDDBA // 0xBADD1D3A on little-endian platforms

#define INDEX_HASH_SIZE 8
#define MAX_DATA_SIZE 16

#define MIN_DATA_BITS 3
#define MAX_DATA_BITS (MAX_DATA_SIZE << 3)

#define MAX_HASH_NAME_SIZE 16

#define INLINE_WORD_MASK 0b1
#define WORD_TYPE_MASK 0b110
#define INLINE_WORD_BITS 1
#define WORD_TYPE_BITS 2
#define TAG_BITS (INLINE_WORD_BITS + WORD_TYPE_BITS)

#define WORD_TYPE_COUNT 4

typedef enum {
    NO_COMPRESSION = 0,
    NUMERIC = 1,
    ALPHANUMERIC = 2,
    REDUCED_ASCII = 3
} WordType;

typedef struct {
    uint32_t magic;
    char hashName[MAX_HASH_NAME_SIZE];
    uint8_t dataBytes;
    uint64_t wordlistOffset;
} __attribute__((packed)) IndexHeader;

uint8_t getMinDataBits(FILE* wordlist);
int isDataSizeValid(FILE* wordlist, uint8_t bits);
uint8_t getIndexEntrySize(IndexHeader* header);
int64_t getIndexesCount(IndexHeader* header);
uint64_t getPointerFromData(uint8_t* data, uint8_t dataBytes);

int readIndexHeader(FILE* in, IndexHeader* header);
int writeIndexHeader(FILE* out, char* hashName, uint8_t dataBytes, uint64_t wordlistOffset);

void writeIndexEntryInline(const uint8_t* hash, const uint8_t* data, size_t compressedDataBits, size_t dataBytes, WordType wordType, FILE* output);
void writeIndexEntryPointer(const uint8_t* hash, uint64_t wordPointer, size_t dataBytes, WordType wordType, FILE* output);

#endif //INDEX_H
