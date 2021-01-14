
#ifndef GPUTRIGGER_GPUTRIGGER_H
#pragma once
#include <sanitizer.h>
#include <sanitizer_result.h>
#include <iostream>
#include <map>
#include <vector>

#include <cstdint>

#include <vector_types.h>

void sanitizer_load_callback(CUcontext context,
                             CUmodule module,
                             const void *cubin,
                             size_t cubin_size);



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
