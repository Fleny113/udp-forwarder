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

    if ((*args->socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[incoming] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(args->port);

    if (bind(*args->socketfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("[incoming] Bind failed\n");
        return EXIT_FAILURE;
    }

    printf("[incoming] Bind on %d\n", args->port);
    fflush(stdout);

    socklen_t len = sizeof(cliaddr);
    int bytesReceived;

    int packetSize = sizeof(packet) - sizeof(packet.data);

    while (1)
    {
        bytesReceived = recvfrom(*args->socketfd, packet.data, sizeof(packet.data), 0, (struct sockaddr *)&cliaddr, &len);

#if DEBUG
        // uint32 -> 4 uint8
        uint8_t *ip = (uint8_t *)&cliaddr.sin_addr.s_addr;

        printf("[incoming] Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], cliaddr.sin_port, bytesReceived);
        fflush(stdout);
#endif

        packet.ip = cliaddr.sin_addr.s_addr;
        packet.port = cliaddr.sin_port;
        packet.length = bytesReceived;

        sendto(*args->fwdFd, &packet, packetSize + bytesReceived, 0, (struct sockaddr *)args->fwdAddr, sizeof(struct sockaddr_in));
    }
}

int serverUDPForward(void *threadArgs)
{
    thrdArgs *args = threadArgs;

    if ((*args->socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[forward] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in servaddr, fwdAddr;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&fwdAddr, 0, sizeof(fwdAddr));

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(args->port);

    fwdAddr.sin_family = AF_INET; // IPv4

    if (bind(*args->socketfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("[forward] Bind failed\n");
        return EXIT_FAILURE;
    }

    printf("[forward] Bind on %d\n", args->port);
    fflush(stdout);

    socklen_t len = sizeof(*args->fwdAddr);
    int bytesReceived;

    int outLen;
    int readSoFar;

    while (1)
    {
        bytesReceived = recvfrom(*args->socketfd, &packet, sizeof(Packet), 0, (struct sockaddr *)args->fwdAddr, &len);

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

int start_server(int incomingPort, int forwardPort)
{
    thrd_t inc, fwd;
    int incomingFd, forwardFd;
    struct sockaddr_in *fwdAddr = malloc(sizeof(struct sockaddr_in));
    memset(fwdAddr, 0, sizeof(struct sockaddr_in));

    thrdArgs incomingArgs, forwardArgs;
    incomingArgs.port = incomingPort;
    incomingArgs.socketfd = &incomingFd;
    incomingArgs.fwdFd = &forwardFd;
    incomingArgs.fwdAddr = fwdAddr;

    forwardArgs.port = forwardPort;
    forwardArgs.socketfd = &forwardFd;
    forwardArgs.fwdFd = &incomingFd;
    forwardArgs.fwdAddr = fwdAddr;

    thrd_create(&inc, &serverUDPIncoming, &incomingArgs);
    thrd_create(&fwd, &serverUDPForward, &forwardArgs);
    thrd_join(inc, NULL);
    thrd_join(fwd, NULL);

    return 0;
}

int main(int argc, char *argv[])
{
    return start_server(8000, 8001);
}
