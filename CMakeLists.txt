cmake_minimum_required(VERSION 3.17)
project(gputrigger)

set(CMAKE_CXX_STANDARD 14)
find_package(CUDAToolkit REQUIRED)


include(CheckLanguage)
check_language(CUDA)
if (NOT CMAKE_CUDA_COMPILER)
    if (${CUDAToolkit_NVCC_EXECUTABLE})
        set(ENV{CUDACXX} ${CUDAToolkit_NVCC_EXECUTABLE})
    elseif (UNIX AND EXISTS "/usr/local/cuda/bin/nvcc")
        set(ENV{CUDACXX} "/usr/local/cuda/bin/nvcc")
        message(WARNING "CMAKE_CUDA_COMPILER guessed: " $ENV{CUDACXX} "\n"
                "Please fix your cuda installation: https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#mandatory-post")
    endif ()
endif ()

enable_language(CUDA)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

set(SANITIZER_PATH ${CUDAToolkit_TARGET_DIR}/compute-sanitizer)
include_directories(${SANITIZER_PATH}/include/ ${CUDAToolkit_TARGET_DIR}/include/)
message(INFO ${SANITIZER_PATH} ${CUDAToolkit_NVCC_EXECUTABLE})

link_libraries(-lsanitizer-public)
add_link_options(-L${SANITIZER_PATH} -fPIC -shared)
add_library(gputrigger SHARED library.cpp)