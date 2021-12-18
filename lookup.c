#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <poll.h>

#include "utils.h"
#include "index.h"
#include "hash.h"
#include "defines.h"

#define UNUSED(x) (void)(x)

typedef struct {
    uint8_t indexEntrySize;
    uint8_t indexDataSize;
    int64_t indexesCount;
    HashInfos hashInfos;
    uint8_t* index;
    uint8_t* wordlist;
} SharedParameters;

static uint32_t childrenRunning = 0;

void childTerminated(int sig)
{
    UNUSED(sig);

    pid_t pid;

    do {
        pid = waitpid((pid_t) -1, NULL, WNOHANG);

        if(pid > 0)
        {
            childrenRunning--;
        }
    } while (pid > 0);
}

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

void lookup(uint8_t* index, uint8_t* wordlist, int64_t indexesCount, uint8_t indexEntrySize, uint8_t indexDataSize,
            HashInfos* hashInfos, uint8_t* digestTmp, uint8_t* hash, uint8_t* out, size_t* outlen)
{
    int64_t l = 0, u = indexesCount - 1, m;
    int cmp;

    *outlen = 0;

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
            while((m >= 0) && (memcmp(index + m * indexEntrySize, hash, INDEX_HASH_SIZE) == 0))
            {
                m--;
            }

            m++;

            while((m < indexesCount) && (memcmp(index + m * indexEntrySize, hash, INDEX_HASH_SIZE) == 0))
            {
                readWord(index + m * indexEntrySize + INDEX_HASH_SIZE, wordlist, indexDataSize, out);
                *outlen = strlen((char*) out);
                hashInfos->f(out, *outlen, digestTmp);

                if(memcmp(hash, digestTmp, hashInfos->digestSize) == 0)
                {
                    return;
                }

                *outlen = 0;
                m++;
            }

            return;
        }
    }
}

int handleClient(int client, SharedParameters* params)
{
    uint8_t lookupResult[MAX_LINE_SIZE];
    char line[MAX_LINE_SIZE];
    size_t lookupResultLen;
    uint8_t* digest, *digestTmp;
    ssize_t readCount;
    struct pollfd fds[1];
    int pollret;

    fds[0].fd = client;
    fds[0].events = POLLIN;

    digest = malloc(params->hashInfos.digestSize);
    digestTmp = malloc(params->hashInfos.digestSize);

    while(1)
    {
        pollret = poll(fds, 1, -1);

        if(pollret == -1)
        {
            goto clienterror;
        }
        else if(pollret > 0)
        {
            readCount = recv(client, line, MAX_LINE_SIZE, 0);

            if(readCount > 0)
            {
                if(line[0] == '\n')
                {
                    break;
                }

                unhex(line, digest, params->hashInfos.digestSize);
                lookup(params->index, params->wordlist, params->indexesCount, params->indexEntrySize,
                       params->indexDataSize, &params->hashInfos, digestTmp, digest, lookupResult, &lookupResultLen);

                if(lookupResultLen == 0)
                {
                    send(client, "\n", sizeof(char), 0);
                }
                else
                {
                    lookupResult[lookupResultLen] = '\n';
                    send(client, lookupResult, lookupResultLen + 1, 0);
                }
            }
            else if(readCount == 0)
            {
                break;
            }
            else
            {
                goto clienterror;
            }
        }
    }

    close(client);

    free(digest);
    free(digestTmp);

    exit(EXIT_SUCCESS);

    clienterror:
    close(client);

    free(digest);
    free(digestTmp);

    exit(EXIT_FAILURE);
}

int serveForever(uint16_t port, uint16_t maxClients, SharedParameters* params)
{
    int server, client;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    uint32_t pid;
    int keepaliveFlag = 1;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    signal(SIGCHLD, &childTerminated);

    server = socket(AF_INET, SOCK_STREAM, 0);

    if(server == 0)
    {
        printf("Unable to create the socket.\n");
        return EXIT_FAILURE;
    }

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) == -1)
    {
        printf("Unable to listen on port %u.\n", port);
        return EXIT_FAILURE;
    }

    printf("The server is listening on port %u for new connections.\n", port);

    while(1)
    {
        if(childrenRunning >= maxClients)
        {
            wait(NULL);
        }

        if(listen(server, 1) == -1)
        {
            perror("An error occurred while listening for a new connection");
            return EXIT_FAILURE;
        }

        if((client = accept(server, (struct sockaddr*) &addr, &addrlen)) == -1)
        {
            perror("An error occurred while accepting a new connection");
        }

        if(setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &keepaliveFlag, sizeof(keepaliveFlag)) == -1)
        {
            perror("Unable to set socket heartbeat for the new client");
        }

        printf("New connection from %s:%u\n", inet_ntoa(addr.sin_addr), addr.sin_port);

        pid = fork();

        if(pid == -1)
        {
            printf("An error occurred during fork.\n");
            close(client);
        }
        else if(pid == 0)
        {
            close(server);
            handleClient(client, params);
        }
        else
        {
            childrenRunning++;
            close(client);
        }
    }

    close(server);
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    uint8_t answer, maxClients;
    uint16_t port;
    uint64_t bufSize;
    FILE* indexFile;
    IndexHeader indexHeader;
    SharedParameters params;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    if(argc != 4)
    {
        printf("Usage: %s <index_file> <port> <max_clients>\n", argv[0]);
        return EXIT_FAILURE;
    }

    port = strtol(argv[2], NULL, 10);
    maxClients = strtol(argv[3], NULL, 10);

    if(port == 0)
    {
        printf("Bad port number: 0.\n");
        return EXIT_FAILURE;
    }

    if(maxClients == 0)
    {
        printf("Max clients set to zero. Changing it to 1.\n");
        maxClients = 1;
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

    params.indexEntrySize = getIndexEntrySize(&indexHeader);
    params.indexDataSize = indexHeader.dataBytes;
    params.indexesCount = getIndexesCount(&indexHeader);

    getHashInfos(indexHeader.hashName, &params.hashInfos);

    if(params.hashInfos.f == NULL)
    {
        printf("Unable to find the hash function named: %s\n", indexHeader.hashName);

        fclose(indexFile);
        return EXIT_FAILURE;
    }

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

    params.index = malloc(bufSize);

    if(params.index == NULL)
    {
        printf("Unable to allocate the index.\n");

        fclose(indexFile);
        return EXIT_FAILURE;
    }

    params.wordlist = params.index + indexHeader.wordlistOffset;
    fread(params.index, sizeof(uint8_t), bufSize, indexFile);

    fclose(indexFile);

    printf("The index is loaded successfully.\n");

    serveForever(port, maxClients, &params);

    free(params.index);

    return EXIT_SUCCESS;
}
