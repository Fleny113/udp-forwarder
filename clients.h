#pragma once
#include <stdint.h>

typedef struct Client
{
    // Ip of the sender
    uint32_t ip;
    // Port of the sender
    uint16_t port;
    // Socket to send / receive messages from
    int sockFd;
    // Linked list next node
    struct Client *next;
} Client;

void initClients();

Client *addClient(uint32_t ip, uint16_t port);
Client *findClientByIpPort(uint32_t ip, uint16_t port);
void removeClient(Client *client);
