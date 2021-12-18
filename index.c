#include "index.h"

uint8_t getMinDataBits(FILE* wordlist)
{
    uint64_t wordlistSize = getFileSize(wordlist);

    return MIN_DATA_BITS + (uint8_t) ceil(log2((double) wordlistSize));
}

int isDataSizeValid(FILE* wordlist, uint8_t bits)
{
    return (bits >= getMinDataBits(wordlist)) && (bits <= MAX_DATA_BITS);
}

uint8_t getIndexEntrySize(IndexHeader* header)
{
    return INDEX_HASH_SIZE + header->dataBytes;
}

int64_t getIndexesCount(IndexHeader* header)
{
    uint8_t indexSize = getIndexEntrySize(header);
    uint64_t indexesSize = header->wordlistOffset;

    // Malformed index
    if(indexesSize % indexSize)
    {
        return 0;
    }

    return (int64_t) (indexesSize / indexSize);
}

uint64_t getPointerFromData(uint8_t* data, uint8_t dataBytes)
{
    uint64_t pointer;

    if (dataBytes > 8)
    {
        pointer = *(uint64_t *) data;
    }
    else if (dataBytes == 8)
    {
        pointer = (*((uint64_t *) data));
        ((uint8_t*) &pointer)[dataBytes - 1] >>= TAG_BITS; // TODO: Check me
    }
    else
    {
        memcpy(&pointer, data, dataBytes);
        ((uint8_t*) &pointer)[dataBytes - 1] >>= TAG_BITS; // TODO: Check me
    }

    return pointer;
}

int readIndexHeader(FILE* in, IndexHeader* header)
{
    fread(header, sizeof(IndexHeader), 1, in);

    if(header->magic != INDEX_MAGIC)
    {
        return 1;
    }

    return getIndexesCount(header) == 0;
}

int writeIndexHeader(FILE* out, char* hashName, uint8_t dataBytes, uint64_t wordlistOffset)
{
    uint32_t magic = INDEX_MAGIC;
    uint8_t hashNameLength = strlen(hashName);
    uint8_t hashNameBuf[MAX_HASH_NAME_SIZE] = {0};

    if(hashNameLength > MAX_HASH_NAME_SIZE)
    {
        return 1;
    }

    memcpy(hashNameBuf, hashName, hashNameLength);

    fwrite(&magic, sizeof(uint32_t), 1, out);
    fwrite(hashNameBuf, sizeof(uint8_t), MAX_HASH_NAME_SIZE, out);
    fwrite(&dataBytes, sizeof(uint8_t), 1, out);
    fwrite(&wordlistOffset, sizeof(uint64_t), 1, out);
}

void writeIndexEntryInline(const uint8_t* hash, const uint8_t* data, size_t compressedDataBits, size_t dataBytes, WordType wordType, FILE* output)
{
    size_t compressedDataBytes = BYTES_SIZE(compressedDataBits);
    size_t paddingBytes = dataBytes - compressedDataBytes;
    uint8_t lastByte = (wordType << 1) | 1;
    uint64_t padding = 0L;

    fwrite(hash, sizeof(uint8_t), INDEX_HASH_SIZE, output);

    if(paddingBytes > 1)
    {
        fwrite(data, sizeof(uint8_t), compressedDataBytes, output);
        fwrite((uint8_t*) &padding, sizeof(uint8_t), paddingBytes - 1, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
    else if(paddingBytes == 1)
    {
        fwrite(data, sizeof(uint8_t), compressedDataBytes, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
    else
    {
        // In this case the last data byte must have at least 3 bits set to 0
        lastByte |= data[dataBytes - 1];

        fwrite(data, sizeof(uint8_t), compressedDataBytes - 1, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
}

void writeIndexEntryPointer(const uint8_t* hash, uint64_t wordPointer, size_t dataBytes, WordType wordType, FILE* output)
{
    uint8_t i, lastByte = wordType << 1;
    size_t compressedDataBytes, paddingBytes;
    uint64_t padding = 0L;

    for(i=0 ; (wordPointer >> i) != 0 ; i++);

    compressedDataBytes = BYTES_SIZE(i);
    paddingBytes = dataBytes - compressedDataBytes;

    fwrite(hash, sizeof(uint8_t), INDEX_HASH_SIZE, output);

    if(paddingBytes > 1)
    {
        fwrite(&wordPointer, sizeof(uint8_t), compressedDataBytes, output);
        fwrite((uint8_t*) &padding, sizeof(uint8_t), paddingBytes - 1, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
    else if(paddingBytes == 1)
    {
        fwrite(&wordPointer, sizeof(uint8_t), compressedDataBytes, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
    else
    {
        // In this case the last data byte must have at least 3 bits set to 0
        lastByte |= (wordPointer >> ((compressedDataBytes - 1) << 3));

        fwrite(&wordPointer, sizeof(uint8_t), compressedDataBytes - 1, output);
        fwrite(&lastByte, sizeof(uint8_t), 1, output);
    }
}