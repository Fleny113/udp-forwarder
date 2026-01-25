#include <stdlib.h>
#include <threads.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "client.h"
#include "clients.h"
#include "packet.h"

int UDPadv(void *threadArgs)
{
    advThrdArgs *args = threadArgs;

    while (1)
    {
        const char message[] = "X";
        sendto(args->socktFd, message, sizeof(message), 0, args->serverAddress, sizeof(struct sockaddr_in));

        sleep(10);
    }

    free(threadArgs);
}

int threadedClientUDPListener(void *threadArgs)
{
    thrdArgs *args = threadArgs;

    if ((args->client->sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[thread] Socket creation failed\n");
        free(threadArgs);
        return EXIT_FAILURE;
    }

    if (connect(args->client->sockFd, args->fwdAddr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("[thread] Connect failed\n");
        free(threadArgs);
        return EXIT_FAILURE;
    }

    if (sendto(args->client->sockFd, args->data, args->length, 0, args->fwdAddr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("[thread] Initial packet send failed\n");
        free(threadArgs);
        return EXIT_FAILURE;
    }

    // Set receive timeout to 30 seconds
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    if (setsockopt(args->client->sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("[thread] setsockopt SO_RCVTIMEO failed\n");
        free(threadArgs);
        return EXIT_FAILURE;
    }

    Packet packet;
    packet.ip = args->client->ip;
    packet.port = args->client->port;
    ssize_t bytesReceived;

#if DEBUG
    // uint32 -> 4 uint8
    uint8_t *ip = (uint8_t *)&packet.ip;
#endif
    int basePacketLength = sizeof(packet) - sizeof(packet.data);

    socklen_t len = sizeof(*args->fwdAddr);

    while (1)
    {
        bytesReceived = recvfrom(args->client->sockFd, &packet.data, sizeof(packet.data), 0, (struct sockaddr *)args->fwdAddr, &len);

        // Check for timeout
        if (bytesReceived < 0)
        {
            const int err = errno;
            if (err == EAGAIN)
            {
#if DEBUG
                // Timeout occurred - connection is stale
                printf("[thread] Connection stale for %d.%d.%d.%d:%d (no data for 30 seconds), closing\n", ip[0], ip[1], ip[2], ip[3], args->client->port);
                fflush(stdout);
#endif
                break;
            }
            else if (err == EINTR)
            {
                // Interrupted by signal, continue receiving
                continue;
            }
            else
            {
                fprintf(stderr, "[thread] recvfrom error %d\n", err);
                fflush(stderr);
                break;
            }
        }

#if DEBUG
        printf("[thread] Server %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], packet.port, bytesReceived);
        fflush(stdout);
#endif

        packet.length = bytesReceived;

        sendto(args->sockFd, &packet, basePacketLength + bytesReceived, 0, args->serverAddress, sizeof(struct sockaddr_in));
    }

#if DEBUG
    printf("Client thread for %d.%d.%d.%d:%d exiting\n", ip[0], ip[1], ip[2], ip[3], args->client->port);
    fflush(stdout);
#endif

    removeClient(args->client);
    close(args->client->sockFd);

    free(threadArgs);
    return 0;
}

int start_client(uint8_t serverAddress[4], uint16_t serverPort, uint8_t forwardAddress[4], uint16_t forwardPort)
{
    int sockFd;
    struct sockaddr_in servAddr;
    struct sockaddr_in *fwdAddr = malloc(sizeof(struct sockaddr_in));

    memset(&servAddr, 0, sizeof(servAddr));
    memset(fwdAddr, 0, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = *(uint32_t *)serverAddress;
    servAddr.sin_port = htons(serverPort);
    fwdAddr->sin_family = AF_INET;
    fwdAddr->sin_addr.s_addr = *(uint32_t *)forwardAddress;
    fwdAddr->sin_port = htons(forwardPort);

    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed\n");
        free(fwdAddr);
        return EXIT_FAILURE;
    }

    if (connect(sockFd, (const struct sockaddr *)&servAddr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("Connect failed\n");
        free(fwdAddr);
        return EXIT_FAILURE;
    }

    printf("Starting background thread to start and keep the connection alive\n");
    fflush(stdout);

    thrd_t advThread;
    advThrdArgs *advArgs = malloc(sizeof(advThrdArgs));
    advArgs->socktFd = sockFd;
    advArgs->serverAddress = (const struct sockaddr *)&servAddr;
    thrd_create(&advThread, &UDPadv, advArgs);

    Packet packet;
    ssize_t bytesReceived;
    Client *c;

#if DEBUG
    // uint32 -> 4 uint8
    uint8_t *ip = (uint8_t *)&packet.ip;
#endif

    while (true)
    {
        bytesReceived = recvfrom(sockFd, &packet, sizeof(Packet), 0, NULL, NULL);

#if DEBUG
        printf("Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], packet.port, bytesReceived);
        fflush(stdout);
#endif

        c = findClientByIpPort(packet.ip, packet.port);
        if (c == NULL)
        {
#if DEBUG
            printf("New client %d.%d.%d.%d:%d, adding to list\n", ip[0], ip[1], ip[2], ip[3], packet.port);
            fflush(stdout);
#endif

            c = addClient(packet.ip, packet.port);

            // spawn thread to listen to this client
            thrdArgs *args = malloc(sizeof(thrdArgs));
            thrd_t clientThread;
            args->client = c;
            args->fwdAddr = (const struct sockaddr *)fwdAddr;
            args->sockFd = sockFd;
            args->serverAddress = (const struct sockaddr *)&servAddr;
            args->length = packet.length;
            memcpy(args->data, packet.data, packet.length);

            thrd_create(&clientThread, &threadedClientUDPListener, args);

            continue;
        }

        sendto(c->sockFd, packet.data, packet.length, 0, (const struct sockaddr *)fwdAddr, sizeof(struct sockaddr_in));
    }
}

int main(int argc, char *argv[])
{
    initClients();

    uint8_t srvIp[] = {127, 0, 0, 1};
    uint8_t fwdIp[] = {192, 168, 1, 6};

    return start_client(srvIp, 8001, fwdIp, 34197);
}
