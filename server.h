#pragma once

typedef struct
{
    // Port for the thread to bind
    int port;
    // Socket of the thread
    int *socketFd;
    // Socket of the other thread
    int *fwdFd;
    // Address of the other thread socket
    struct sockaddr_in *fwdAddr;
} thrdArgs;
