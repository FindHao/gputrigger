
#include "gputrigger.h"

#include <sanitizer.h>

#include <iostream>
#include <map>
#include <vector>

using namespace std;
// TODO: handle multiple contexts

// TODO: allow override of array size through env variable

// TODO: write in a file instead of stdout

struct LaunchData
{
    std::string functionName;
    MemoryAccessTracker *pTracker;
};

struct CallbackTracker
{
    std::map<Sanitizer_StreamHandle, std::vector<LaunchData>> memoryTrackers;
};

void ModuleLoaded(CUcontext context,
                  CUmodule module,
                  const void *cubin,
                  size_t cubin_size)
{
    // Instrument user code!
    sanitizerAddPatchesFromFile("gputrigger_patch.fatbin", context);
    sanitizerPatchInstructions(SANITIZER_INSTRUCTION_GLOBAL_MEMORY_ACCESS, module, "MemoryGlobalAccessCallback");
    sanitizerPatchModule(module);
}

void LaunchBegin(
        CallbackTracker *pCallbackTracker,
        CUcontext context,
        CUfunction function,
        std::string functionName,
        Sanitizer_StreamHandle stream)
{
    constexpr size_t MemAccessDefaultSize = 1024;

    // alloc MemoryAccess array
    MemoryAccess *accesses = nullptr;
    sanitizerAlloc(context, (void **)&accesses, sizeof(MemoryAccess) * MemAccessDefaultSize);
    sanitizerMemset(accesses, 0, sizeof(MemoryAccess) * MemAccessDefaultSize, stream);

    MemoryAccessTracker hTracker;
    hTracker.currentEntry = 0;
    hTracker.maxEntry = MemAccessDefaultSize;
    hTracker.accesses = accesses;

    MemoryAccessTracker *dTracker = nullptr;
    sanitizerAlloc(context, (void **)&dTracker, sizeof(*dTracker));
    sanitizerMemcpyHostToDeviceAsync(dTracker, &hTracker, sizeof(*dTracker), stream);

    sanitizerSetCallbackData(function, dTracker);

    LaunchData launchData = {functionName, dTracker};
    cout<<"Launch\t"<< functionName<<endl;
    std::vector<LaunchData> &deviceTrackers = pCallbackTracker->memoryTrackers[stream];
    deviceTrackers.push_back(launchData);
}

static std::string GetMemoryRWString(uint32_t flags)
{
    if (flags & SANITIZER_MEMORY_DEVICE_FLAG_READ)
    {
        return "Read";
    }
    else if (flags & SANITIZER_MEMORY_DEVICE_FLAG_WRITE)
    {
        return "Write";
    }
    else
    {
        return "Unknown";
    }
}

static std::string GetMemoryTypeString(MemoryAccessType type)
{
    if (type == MemoryAccessType::Local)
    {
        return "local";
    }
    else if (type == MemoryAccessType::Shared)
    {
        return "shared";
    }
    else
    {
        return "global";
    }
}

void StreamSynchronized(
        CallbackTracker *pCallbackTracker,
        CUcontext context,
        Sanitizer_StreamHandle stream)
{
    MemoryAccessTracker hTracker = {0};

    std::vector<LaunchData> &deviceTrackers = pCallbackTracker->memoryTrackers[stream];

    for (auto &tracker : deviceTrackers)
    {
        std::cout << "Kernel Launch: " << tracker.functionName << std::endl;

        sanitizerMemcpyDeviceToHost(&hTracker, tracker.pTracker, sizeof(*tracker.pTracker), stream);

        uint32_t numEntries = std::min(hTracker.currentEntry, hTracker.maxEntry);

        std::cout << "  Memory accesses: " << numEntries << std::endl;

        std::vector<MemoryAccess> accesses(numEntries);
        sanitizerMemcpyDeviceToHost(accesses.data(), hTracker.accesses, sizeof(MemoryAccess) * numEntries, stream);

        for (uint32_t i = 0; i < numEntries; ++i)
        {
            MemoryAccess &access = accesses[i];

            std::cout << "  [" << i << "] " << GetMemoryRWString(access.flags)
                      << " access of " << GetMemoryTypeString(access.type)
                      << " memory by thread (" << access.threadId.x
                      << "," << access.threadId.y
                      << "," << access.threadId.z
                      << ") at address 0x" << std::hex << access.address << std::dec
                      << " (size is " << access.accessSize << " bytes)" << std::endl;
        }

        sanitizerFree(context, hTracker.accesses);
        sanitizerFree(context, tracker.pTracker);
    }

    deviceTrackers.clear();
}

void ContextSynchronized(CallbackTracker *pCallbackTracker, CUcontext context)
{
    for (auto &streamTracker : pCallbackTracker->memoryTrackers)
    {
        StreamSynchronized(pCallbackTracker, context, streamTracker.first);
    }
}

void MemoryTrackerCallback(
        void *userdata,
        Sanitizer_CallbackDomain domain,
        Sanitizer_CallbackId cbid,
        const void *cbdata)
{
    cout << "tracker\t"<<domain<<endl;

    auto *callbackTracker = (CallbackTracker *)userdata;

    switch (domain)
    {
        case SANITIZER_CB_DOMAIN_RESOURCE:
            cout << "SANITIZER_CB_DOMAIN_RESOURCE resource " << cbid << endl;
            switch (cbid)
            {
                case SANITIZER_CBID_RESOURCE_MODULE_LOADED:
                {
                    auto *md = (Sanitizer_ResourceModuleData *)cbdata;
                    cout<<"===module load==="<<endl;
                    ModuleLoaded(md->context, md->module, md->pCubin, md->cubinSize);
                    break;
                }
                case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_ALLOC:
                {
                    Sanitizer_ResourceMemoryData *med = (Sanitizer_ResourceMemoryData *)cbdata;

                    cout<<"device memory alloc"<<endl;
                    break;
                }
                case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_FREE:
                {
                    cout<<"device memory free"<<endl;
                    break;
                }
                default:
                    break;
            }
            break;
        case SANITIZER_CB_DOMAIN_LAUNCH:
            switch (cbid)
            {
                case SANITIZER_CBID_LAUNCH_BEGIN:
                {
                    auto *pLaunchData = (Sanitizer_LaunchData *)cbdata;
                    LaunchBegin(callbackTracker, pLaunchData->context, pLaunchData->function, pLaunchData->functionName, pLaunchData->hStream);
                    break;
                }
                default:
                    break;
            }
            break;
        case SANITIZER_CB_DOMAIN_SYNCHRONIZE:
            switch (cbid)
            {
                case SANITIZER_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED:
                {
                    auto *pSyncData = (Sanitizer_SynchronizeData *)cbdata;
                    StreamSynchronized(callbackTracker, pSyncData->context, pSyncData->hStream);
                    break;
                }
                case SANITIZER_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED:
                {
                    auto *pSyncData = (Sanitizer_SynchronizeData *)cbdata;
                    ContextSynchronized(callbackTracker, pSyncData->context);
                    break;
                }
                default:
                    break;
            }
            break;
        case SANITIZER_CB_DOMAIN_UVM:
            cout<<"uvm======="<<endl;
            cout<<cbid<<endl;
            break;
        default:
            break;
    }
}

int InitializeInjection()
{
    Sanitizer_SubscriberHandle handle;
    CallbackTracker *tracker = new CallbackTracker();

    sanitizerSubscribe(&handle, MemoryTrackerCallback, tracker);
//    sanitizerEnableAllDomains(1, handle);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_LAUNCH);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_UVM);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_RESOURCE);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_MEMCPY);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_MEMSET);
     sanitizerEnableDomain(1, handle, SANITIZER_CB_DOMAIN_SYNCHRONIZE);

    return 0;
}

int __global_initializer__ = InitializeInjection();
