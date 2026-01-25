#include <stdlib.h>
#include <threads.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "server.h"
#include "packet.h"

int serverUDPIncoming(void *threadArgs)
{
    thrdIncomingArgs *args = threadArgs;

    if ((*args->socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[incoming] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in servAddr, cliAddr;

    memset(&servAddr, 0, sizeof(servAddr));
    memset(&cliAddr, 0, sizeof(cliAddr));

    servAddr.sin_family = AF_INET;
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
    const int packetSize = sizeof(packet) - sizeof(packet.data);

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

        sendto(*args->forwardFd, &packet, packetSize + bytesReceived, 0, (struct sockaddr *)args->fowardAddress, sizeof(*args->fowardAddress));
    }
}

int serverUDPForward(void *threadArgs)
{
    thrdForwardArgs *args = threadArgs;

    if ((*args->socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[forward] Socket creation failed\n");
        return EXIT_FAILURE;
    }

    Packet packet;

    struct sockaddr_in serverAddress, receiveAddress, incomingAddress;

    memset(&serverAddress, 0, sizeof(serverAddress));
    memset(&receiveAddress, 0, sizeof(receiveAddress));
    memset(&incomingAddress, 0, sizeof(incomingAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(args->port);

    incomingAddress.sin_family = AF_INET;

    if (bind(*args->socketFd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("[forward] Bind failed\n");
        return EXIT_FAILURE;
    }

    printf("[forward] Bind on %d\n", args->port);
    fflush(stdout);

    socklen_t len = sizeof(receiveAddress);
    const size_t advPacketLength = sizeof(packet.ip) + sizeof(packet.port) + sizeof(packet.length) + args->passwordLength;

    while (true)
    {
        const ssize_t bytesReceived = recvfrom(*args->socketFd, &packet, sizeof(Packet), 0, (struct sockaddr *)&receiveAddress, &len);

#if DEBUG
        // uint32 -> 4 uint8
        uint8_t *ip = (uint8_t *)&receiveAddress.sin_addr.s_addr;

        printf("[forward] Client %d.%d.%d.%d:%d: Received %d bytes\n", ip[0], ip[1], ip[2], ip[3], receiveAddress.sin_port, bytesReceived);
        fflush(stdout);
#endif

        if (packet.ip == 0 && packet.port == 0 && bytesReceived == advPacketLength)
        {
#if DEBUG
            printf("[forward] Received password in adv packet\n");
            fflush(stdout);
#endif
            if (packet.length == args->passwordLength && strcmp(packet.data, args->password) == 0)
            {
#if DEBUG
                printf("[forward] Valid password in adv packet\n");
                fflush(stdout);
#endif

                args->fowardAddress->sin_addr.s_addr = receiveAddress.sin_addr.s_addr;
                args->fowardAddress->sin_port = receiveAddress.sin_port;
            }
            else
            {
#if DEBUG
                printf("[forward] Invalid password in adv packet\n");
                fflush(stdout);
#endif
            }

            continue;
        }

        incomingAddress.sin_addr.s_addr = packet.ip;
        incomingAddress.sin_port = packet.port;

        sendto(*args->incomingFd, packet.data, packet.length, 0, (struct sockaddr *)&incomingAddress, len);
    }
}

int start_server(const int incomingPort, const int forwardPort, const char *password)
{
    thrd_t inc, fwd;
    int incomingFd, forwardFd;
    struct sockaddr_in *forwardAddress = malloc(sizeof(struct sockaddr_in));
    memset(forwardAddress, 0, sizeof(struct sockaddr_in));

    thrdIncomingArgs incomingArgs;
    incomingArgs.port = incomingPort;
    incomingArgs.socketFd = &incomingFd;
    incomingArgs.forwardFd = &forwardFd;
    incomingArgs.fowardAddress = forwardAddress;

    thrdForwardArgs forwardArgs;
    forwardArgs.port = forwardPort;
    forwardArgs.socketFd = &forwardFd;
    forwardArgs.incomingFd = &incomingFd;
    forwardArgs.fowardAddress = forwardAddress;
    forwardArgs.password = password;
    forwardArgs.passwordLength = strlen(password);

    thrd_create(&inc, &serverUDPIncoming, &incomingArgs);
    thrd_create(&fwd, &serverUDPForward, &forwardArgs);
    thrd_join(inc, NULL);
    thrd_join(fwd, NULL);

    return 0;
}

int main(int argc, char *argv[])
{
    return start_server(8000, 8001, "secret");
}
