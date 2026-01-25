#pragma once

typedef struct
{
    // Port for the thread to bind
    uint16_t port;
    // Socket for the incoming messages from clients
    int *socketFd;
    // Socket for the forwarded messages
    const int *forwardFd;
    // Address of the forwarded socket
    const struct sockaddr_in *fowardAddress;
} thrdIncomingArgs;

typedef struct
{
    // Port for the thread to bind
    uint16_t port;
    // Socket for the forwarded messages
    int *socketFd;
    // Socket for the incoming messages from clients
    const int *incomingFd;
    // Address of the forwarded socket
    struct sockaddr_in *fowardAddress;
    // Password for advertisement packets
    const char *password;
    // Length of the password
    size_t passwordLength;
} thrdForwardArgs;
