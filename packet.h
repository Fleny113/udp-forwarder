#pragma once
#include <stdint.h>

typedef struct
{
    // Ip of the original sender
    uint32_t ip;
    // Port of the original sender
    uint16_t port;
    // Length of the following data section
    uint16_t length;
    // Data buffer
    uint8_t data[4096];
} Packet;
