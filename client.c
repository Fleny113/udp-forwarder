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
    thrdAdvArgs *args = threadArgs;

    Packet advPacket;
    advPacket.ip = 0;
    advPacket.port = 0;
    advPacket.length = args->passwordLength;
    memcpy(advPacket.data, args->password, args->passwordLength);

    const size_t advPacketSize = sizeof(advPacket.ip) + sizeof(advPacket.port) + sizeof(advPacket.length) + args->passwordLength;

    while (1)
    {
        sendto(args->socktFd, &advPacket, advPacketSize, 0, args->serverAddress, sizeof(struct sockaddr_in));
        sleep(10);
    }

    free(threadArgs);
}

int threadedClientUDPListener(void *threadArgs)
{
    thrdClientArgs *args = threadArgs;

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

int start_client(uint8_t serverIp[4], uint16_t serverPort, uint8_t forwardIp[4], uint16_t forwardPort, const char *password)
{
    int sockFd;
    struct sockaddr_in serverAddress;
    struct sockaddr_in *forwardAddress = malloc(sizeof(struct sockaddr_in));

    memset(&serverAddress, 0, sizeof(serverAddress));
    memset(forwardAddress, 0, sizeof(struct sockaddr_in));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = *(uint32_t *)serverIp;
    serverAddress.sin_port = htons(serverPort);

    forwardAddress->sin_family = AF_INET;
    forwardAddress->sin_addr.s_addr = *(uint32_t *)forwardIp;
    forwardAddress->sin_port = htons(forwardPort);

    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed\n");
        free(forwardAddress);
        return EXIT_FAILURE;
    }

    if (connect(sockFd, (const struct sockaddr *)&serverAddress, sizeof(struct sockaddr_in)) < 0)
    {
        perror("Connect failed\n");
        free(forwardAddress);
        return EXIT_FAILURE;
    }

    printf("Starting background thread to start and keep the connection alive\n");
    fflush(stdout);

    thrdAdvArgs *advArgs = malloc(sizeof(thrdAdvArgs));
    advArgs->socktFd = sockFd;
    advArgs->serverAddress = (const struct sockaddr *)&serverAddress;
    advArgs->password = password;
    advArgs->passwordLength = strlen(password);

    thrd_t advThread;
    thrd_create(&advThread, &UDPadv, advArgs);

    Packet packet;

#if DEBUG
    // uint32 -> 4 uint8
    uint8_t *ip = (uint8_t *)&packet.ip;
#endif

    while (true)
    {
        const ssize_t bytesReceived = recvfrom(sockFd, &packet, sizeof(Packet), 0, NULL, NULL);

#if DEBUG
        printf("Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], packet.port, bytesReceived);
        fflush(stdout);
#endif

        Client *c = findClientByIpPort(packet.ip, packet.port);
        if (c == NULL)
        {
#if DEBUG
            printf("New client %d.%d.%d.%d:%d, adding to list\n", ip[0], ip[1], ip[2], ip[3], packet.port);
            fflush(stdout);
#endif

            c = addClient(packet.ip, packet.port);

            // spawn thread to listen to this client
            thrdClientArgs *args = malloc(sizeof(thrdClientArgs));

            args->client = c;
            args->fwdAddr = (const struct sockaddr *)forwardAddress;
            args->sockFd = sockFd;
            args->serverAddress = (const struct sockaddr *)&serverAddress;
            args->length = packet.length;
            memcpy(args->data, packet.data, packet.length);

            thrd_t clientThread;
            thrd_create(&clientThread, &threadedClientUDPListener, args);

            continue;
        }

        sendto(c->sockFd, packet.data, packet.length, 0, (const struct sockaddr *)forwardAddress, sizeof(struct sockaddr_in));
    }
}

int main(int argc, char *argv[])
{
    initClients();

    if (argc != 6)
    {
        printf("Usage: %s <server ip> <server port> <forward ip> <forward port> <password>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t serverIp[4];
    uint8_t forwardIp[4];

    if (sscanf(argv[1], "%hd.%hd.%hd.%hd", &serverIp[0], &serverIp[1], &serverIp[2], &serverIp[3]) != 4)
    {
        printf("Invalid server IP address\n");
        return EXIT_FAILURE;
    }

    uint16_t serverPort = atoi(argv[2]);

    if (sscanf(argv[3], "%hd.%hd.%hd.%hd", &forwardIp[0], &forwardIp[1], &forwardIp[2], &forwardIp[3]) != 4)
    {
        printf("Invalid forward IP address\n");
        return EXIT_FAILURE;
    }

    uint16_t forwardPort = atoi(argv[4]);
    char *password = argv[5];

    return start_client(serverIp, serverPort, forwardIp, forwardPort, password);
}
