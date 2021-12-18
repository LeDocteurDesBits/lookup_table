#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BYTES_SIZE(bitsSize) (((bitsSize) >> 3) + (((bitsSize) % 8) != 0))

#define NUMERIC_COMPRESSED_BITS(n) ((n) << 2)
#define ALPHANUMERIC_COMPRESSED_BITS(n) ((n) * 6)
#define REDUCED_ASCII_COMPRESSED_BITS(n) ((n) * 7)

#define NUMERIC_STOP_SYMBOL 0b1111
#define ALPHANUMERIC_STOP_SYMBOL 0b111111
#define REDUCED_ASCII_STOP_SYMBOL 0b1111111

#define NUMERIC_SYMBOL_BITS 4
#define ALPHANUMERIC_SYMBOL_BITS 6
#define REDUCED_ASCII_SYMBOL_BITS 7

uint64_t getFileSize(FILE* f);

int isAlphanumeric(char* s);
int isReducedASCII(char* s);
int isNumeric(char* s);

void compressNumeric(char* s, size_t n, uint8_t* out);
void compressAlphanumeric(char* s, size_t n, uint8_t* out);
void compressReducedASCII(char* s, size_t n, uint8_t* out);

void uncompressNumeric(uint8_t* c, uint8_t* out);
void uncompressAlphanumeric(uint8_t* c, uint8_t* out);
void uncompressReducedASCII(uint8_t* c, uint8_t* out);

void unhex(char* hex, uint8_t* out, size_t n);

#endif //UTILS_H
