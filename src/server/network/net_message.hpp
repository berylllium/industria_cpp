#pragma once

#include <cstdint>
#include <vector>

struct NetMessageHeader
{
    uint32_t id;
    uint32_t body_size;
};

struct NetMessage
{
    NetMessageHeader header;
    std::vector<uint8_t> body;


};
