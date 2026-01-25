#pragma once
#include <stdint.h>
#include <sys/socket.h>
#include "clients.h"

typedef struct
{
    // Address of the destination server
    const struct sockaddr *fwdAddr;
    // Length of the following data section
    uint16_t length;
    // Data buffer for the initial packet
    uint8_t data[4096];
    // Socket of the forwarding server
    int sockFd;
    // Address of the server to connect to
    const struct sockaddr *serverAddress;
    // Client info
    Client *client;
} thrdClientArgs;

typedef struct
{
    // Socket to send advertising packets from
    int socktFd;
    // Address of the server to send advertising packets to
    const struct sockaddr *serverAddress;
    // Password for advertisement packets
    const char *password;
    // Length of the password
    size_t passwordLength;
} thrdAdvArgs;
