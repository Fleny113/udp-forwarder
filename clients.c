#include <stdlib.h>
#include <semaphore.h>
#include "clients.h"

Client *clients = NULL;
sem_t mutex;

void initClients()
{
    sem_init(&mutex, 0, 1);
}

Client *addClient(uint32_t ip, uint16_t port)
{
    sem_wait(&mutex);

    if (clients == NULL)
    {
        Client *root = clients = malloc(sizeof(Client));
        root->ip = ip;
        root->port = port;
        root->sockFd = -1;
        root->next = NULL;
        sem_post(&mutex);
        return root;
    }

    Client *root = clients;
    while (root->next != NULL)
    {
        root = root->next;
    }

    root->next = malloc(sizeof(Client));

    root = root->next;
    root->ip = ip;
    root->port = port;
    root->sockFd = -1;
    root->next = NULL;

    sem_post(&mutex);

    return root;
}

Client *findClientByIpPort(uint32_t ip, uint16_t port)
{
    sem_wait(&mutex);
    Client *root = clients;

    while (root != NULL)
    {
        if (root->ip == ip && root->port == port)
        {
            sem_post(&mutex);
            return root;
        }
        root = root->next;
    }

    sem_post(&mutex);
    return NULL;
}

void removeClient(Client *client)
{
    sem_wait(&mutex);

    Client *root = clients;
    Client *prev = NULL;

    while (root != NULL)
    {
        if (root == client)
        {
            if (prev == NULL)
            {
                clients = root->next;
            }
            else
            {
                prev->next = root->next;
            }
            free(root);
            sem_post(&mutex);
            return;
        }
        prev = root;
        root = root->next;
    }

    sem_post(&mutex);
}
