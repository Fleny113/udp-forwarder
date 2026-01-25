#include <stdlib.h>
#include <threads.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "server.h"
#include "packet.h"

int serverUDPIncoming(void *threadArgs)
{
    thrdArgs *args = threadArgs;

    if ((*args->socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[incoming] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in servAddr = {0}, cliAddr = {0};

    servAddr.sin_family = AF_INET; // IPv4
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(args->port);

    if (bind(*args->socketFd, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("[incoming] Bind failed\n");
        return EXIT_FAILURE;
    }

    printf("[incoming] Bind on %d\n", args->port);
    fflush(stdout);

    socklen_t len = sizeof(cliAddr);

    constexpr int packetSize = sizeof(packet) - sizeof(packet.data);

    while (true)
    {
        const ssize_t bytesReceived = recvfrom(*args->socketFd, packet.data, sizeof(packet.data), 0, (struct sockaddr *)&cliAddr, &len);

#if DEBUG
        // uint32 -> 4 uint8
        uint8_t *ip = (uint8_t *)&cliAddr.sin_addr.s_addr;

        printf("[incoming] Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], cliAddr.sin_port, bytesReceived);
        fflush(stdout);
#endif

        packet.ip = cliAddr.sin_addr.s_addr;
        packet.port = cliAddr.sin_port;
        packet.length = bytesReceived;

        sendto(*args->fwdFd, &packet, packetSize + bytesReceived, 0, (struct sockaddr *)args->fwdAddr, sizeof(struct sockaddr_in));
    }
}

int serverUDPForward(void *threadArgs)
{
    thrdArgs *args = threadArgs;

    if ((*args->socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[forward] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in servAddr = {0}, fwdAddr = {0};

    servAddr.sin_family = AF_INET; // IPv4
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(args->port);

    fwdAddr.sin_family = AF_INET; // IPv4

    if (bind(*args->socketFd, (const struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("[forward] Bind failed\n");
        return EXIT_FAILURE;
    }

    printf("[forward] Bind on %d\n", args->port);
    fflush(stdout);

    socklen_t len = sizeof(*args->fwdAddr);


    int outLen;
    int readSoFar;

    while (true)
    {
        const ssize_t bytesReceived = recvfrom(*args->socketFd, &packet, sizeof(Packet), 0, (struct sockaddr *)args->fwdAddr, &len);

#if DEBUG
        // uint32 -> 4 uint8
        uint8_t *ip = (uint8_t *)&args->fwdAddr->sin_addr.s_addr;

        printf("[forward] Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], args->fwdAddr->sin_port, bytesReceived);
        fflush(stdout);
#endif

        if (bytesReceived < 4)
        {
#if DEBUG
            printf("[forward] Received small packet: %d bytes, assuming adv\n", bytesReceived);
            fflush(stdout);
#endif
            continue;
        }

        fwdAddr.sin_addr.s_addr = packet.ip;
        fwdAddr.sin_port = packet.port;

        sendto(*args->fwdFd, packet.data, packet.length, 0, (struct sockaddr *)&fwdAddr, len);
    }
}

int start_server(const int incomingPort, const int forwardPort)
{
    thrd_t inc, fwd;
    int incomingFd, forwardFd;
    struct sockaddr_in *fwdAddr = malloc(sizeof(struct sockaddr_in));
    memset(fwdAddr, 0, sizeof(struct sockaddr_in));

    thrdArgs incomingArgs, forwardArgs;
    incomingArgs.port = incomingPort;
    incomingArgs.socketFd = &incomingFd;
    incomingArgs.fwdFd = &forwardFd;
    incomingArgs.fwdAddr = fwdAddr;

    forwardArgs.port = forwardPort;
    forwardArgs.socketFd = &forwardFd;
    forwardArgs.fwdFd = &incomingFd;
    forwardArgs.fwdAddr = fwdAddr;

    thrd_create(&inc, &serverUDPIncoming, &incomingArgs);
    thrd_create(&fwd, &serverUDPForward, &forwardArgs);
    thrd_join(inc, nullptr);
    thrd_join(fwd, nullptr);

    return 0;
}

int main(int argc, char *argv[])
{
    return start_server(8000, 8001);
}
