
#include "gputrigger.h"
#include <filesystem>
#include<experimental/filesystem>

#if SANITIZER_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define GPUPUNK_SANITIZER_CALL(fn, args...) wrapped_call(fn, #fn, ##args)

template<typename T, typename ...Args>
void wrapped_call(T *f, const char *fn, Args... args) {
    SanitizerResult status = f(std::forward<Args>(args)...);
    if (status != SANITIZER_SUCCESS) {
        const char *error_string;
        sanitizerGetResultString(status, &error_string);
        PRINT("Sanitizer result error: function %s failed with error %s\\n", fn, error_string);
    }
}

struct LaunchData {
    std::string functionName;
    MemoryAccessTracker *pTracker;
};

struct CallbackTracker {
    std::map<Sanitizer_StreamHandle, std::vector<LaunchData>> memoryTrackers;
};


void sanitizer_load_callback(CUcontext context, CUmodule module, const void *cubin, size_t cubin_size) {
    using std::experimental::filesystem::exists;
    using std::cout;
    using std::endl;
//    check patch file
    const char *env_FATBIN_PATCH = std::getenv("GPUPUNK_PATCH");
    cout << env_FATBIN_PATCH;
    if (not exists(env_FATBIN_PATCH)) {
        cout << " does not exist";
        exit(-1);
    }
    PRINT("Patch CUBIN: \n");
    // Instrument user code!
    GPUPUNK_SANITIZER_CALL(sanitizerAddPatchesFromFile, env_FATBIN_PATCH, context);
    GPUPUNK_SANITIZER_CALL(sanitizerPatchInstructions, SANITIZER_INSTRUCTION_GLOBAL_MEMORY_ACCESS, module,
                           "sanitizer_global_memory_access_callback");
    GPUPUNK_SANITIZER_CALL(sanitizerPatchInstructions, SANITIZER_INSTRUCTION_SHARED_MEMORY_ACCESS, module,
                           "sanitizer_shared_memory_access_callback");
    GPUPUNK_SANITIZER_CALL(sanitizerPatchInstructions, SANITIZER_INSTRUCTION_LOCAL_MEMORY_ACCESS, module,
                           "sanitizer_local_memory_access_callback");
    GPUPUNK_SANITIZER_CALL(sanitizerPatchInstructions, SANITIZER_INSTRUCTION_BLOCK_ENTER, module,
                           "sanitizer_block_enter_callback");
    GPUPUNK_SANITIZER_CALL(sanitizerPatchInstructions, SANITIZER_INSTRUCTION_BLOCK_EXIT, module,
                           "sanitizer_block_exit_callback");
    GPUPUNK_SANITIZER_CALL(sanitizerPatchModule, module);
}

void gpupunk_memory_register_trigger(Sanitizer_ResourceMemoryData * md){

}
void gpupunk_memory_unregister_trigger(Sanitizer_ResourceMemoryData * md){}

void MemoryTrackerCallback(
        void *userdata,
        Sanitizer_CallbackDomain domain,
        Sanitizer_CallbackId cbid,
        const void *cbdata) {
    using std::cout;
    using std::endl;
    cout << "tracker\t" << domain << endl;

    static __thread CUcontext sanitizer_thread_context = nullptr;

    if (domain == SANITIZER_CB_DOMAIN_RESOURCE) {
        cout << "SANITIZER_CB_DOMAIN_RESOURCE resource " << cbid << endl;
        switch (cbid) {
            case SANITIZER_CBID_RESOURCE_MODULE_LOADED: {
                auto *md = (Sanitizer_ResourceModuleData *) cbdata;
                sanitizer_load_callback(md->context, md->module, md->pCubin, md->cubinSize);
                break;
            }
            case SANITIZER_CBID_RESOURCE_STREAM_CREATED: {
//                    @todo
                break;
            }
            case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_ALLOC: {
                Sanitizer_ResourceMemoryData *md = (Sanitizer_ResourceMemoryData *) cbdata;
                sanitizer_thread_context = md->context;
                gpupunk_memory_register_trigger(md);
                break;
            }
            case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_FREE: {
                Sanitizer_ResourceMemoryData *md = (Sanitizer_ResourceMemoryData *) cbdata;
                sanitizer_thread_context = md->context;
                gpupunk_memory_unregister_trigger(md);
                cout << "device memory free" << endl;
                break;
            }
            default:
                break;
        }
    } else if (domain == SANITIZER_CB_DOMAIN_LAUNCH) {
        Sanitizer_LaunchData *ld = (Sanitizer_LaunchData *) cbdata;
        sanitizer_thread_context = ld->context;
        if (cbid == SANITIZER_CBID_LAUNCH_BEGIN) {
            auto *pLaunchData = (Sanitizer_LaunchData *) cbdata;
//                  gpupunk_kernel_begin(sanitizer_thread_id_local, persistent_id, correlation_id);
        } else if (cbid == SANITIZER_CBID_LAUNCH_END) {
//      gpupunk_kernel_end(sanitizer_thread_id_local, persistent_id, correlation_id);
        }
    } else if (domain == SANITIZER_CB_DOMAIN_MEMCPY) {
        Sanitizer_MemcpyData *md = (Sanitizer_MemcpyData *) cbdata;
        sanitizer_thread_context = md->srcContext;
        // Avoid memcpy to symbol without allocation
        // Let gpupunk update shadow memory
//        gpupunk_memcpy_register(persistent_id, correlation_id, src_host, md->srcAddress, dst_host, md->dstAddress, md->size);
    } else if (domain == SANITIZER_CB_DOMAIN_MEMSET) {
        Sanitizer_MemsetData *md = (Sanitizer_MemsetData *) cbdata;
        sanitizer_thread_context = md->context;
//    gpupunk_memset_register(persistent_id, correlation_id, md->address, md->value, md->width);
    } else if (domain == SANITIZER_CB_DOMAIN_SYNCHRONIZE) {
    } else if (domain == SANITIZER_CB_DOMAIN_UVM) {
        cout << "uvm=======" << endl;
        cout << cbid << endl;
    }
}

int InitializeInjection() {
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
