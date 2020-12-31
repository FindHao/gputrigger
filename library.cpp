/* Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sanitizer_callbacks.h>

// CUDA include for cudaError_t
#include <driver_types.h>

#include <iostream>

static void gpupunk_test1(Sanitizer_LaunchData* cbdata){

}

static void ApiTrackerCallback(
        void* userdata,
        Sanitizer_CallbackDomain domain,
        Sanitizer_CallbackId cbid,
        const void* cbdata)
{
    using std::cout;
    using std::endl;
//    if (domain != SANITIZER_CB_DOMAIN_RUNTIME_API && domain != SANITIZER_CB_DOMAIN_LAUNCH)
//        return;
//
//    auto* pCallbackData = (Sanitizer_CallbackData*)cbdata;
//    if(cbid == SANITIZER_CBID_LAUNCH_BEGIN){
//        cout<<"Lunch begin : ==="<< pCallbackData->functionName <<"==="<<endl;
//    }
//
//    if (pCallbackData->callbackSite == SANITIZER_API_ENTER){
//        cout<<"SANITIZER_API_ENTER"  << "API call to " << pCallbackData->functionName  << std::endl;
//        a();
//    }
//    else{
//        auto returnValue = *(cudaError_t*)pCallbackData->functionReturnValue;
//        cout << "API call to " << pCallbackData->functionName << " (return code "
//                << returnValue << ")" << std::endl;
//
//    }


    switch (domain)
    {
        case SANITIZER_CB_DOMAIN_RESOURCE:
            switch (cbid)
            {
                case SANITIZER_CBID_RESOURCE_MODULE_LOADED:
                {
//                    auto* pModuleData = (Sanitizer_ResourceModuleData*)cbdata;
//                    ModuleLoaded(pModuleData);
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
                    auto* pLaunchData = (Sanitizer_LaunchData*)cbdata;
                    cout<<"Lunch begin : ==="<< pLaunchData->functionName <<"==="<<endl;
                    gpupunk_test1(pLaunchData);
                    break;
                }
                default:
                    break;
            }
            break;
        case SANITIZER_CB_DOMAIN_SYNCHRONIZE:
//            switch (cbid)
//            {
//                case SANITIZER_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED:
//                {
//                    auto* pSyncData = (Sanitizer_SynchronizeData*)cbdata;
//                    StreamSynchronized(callbackTracker, pSyncData->context, pSyncData->hStream);
//                    break;
//                }
//                case SANITIZER_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED:
//                {
//                    auto* pSyncData = (Sanitizer_SynchronizeData*)cbdata;
//                    ContextSynchronized(callbackTracker, pSyncData->context);
//                    break;
//                }
//                default:
//                    break;
//            }
            break;
        default:
            break;
    }
}

int InitializeInjection()
{
    Sanitizer_SubscriberHandle handle;

    sanitizerSubscribe(&handle, ApiTrackerCallback, nullptr);
    sanitizerEnableAllDomains(1, handle);

    return 0;
}

int __global_initializer__ = InitializeInjection();