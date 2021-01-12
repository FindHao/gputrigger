//
// Created by findhao on 1/7/21.
//

#ifndef GPUTRIGGER_GPUTRIGGER_H
#pragma once

#include <cstdint>

#include <vector_types.h>

enum class MemoryAccessType
{
    Global,
    Shared,
    Local,
};

// Information regarding a memory access
struct MemoryAccess
{
    uint64_t address;
    uint32_t accessSize;
    uint32_t flags;
    dim3     threadId;
    MemoryAccessType type;
};

// Main tracking structure that patches get as userdata
struct MemoryAccessTracker
{
    uint32_t currentEntry;
    uint32_t maxEntry;
    MemoryAccess* accesses;
};

#define GPUTRIGGER_GPUTRIGGER_H

#endif //GPUTRIGGER_GPUTRIGGER_H
