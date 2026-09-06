cmake_minimum_required(VERSION 3.22.1)
project(kfolcalc)

add_library(kfolcalc SHARED
    jni_bridge.cpp
    kfol_core.cpp
    kfol_cpu.cpp
    kfol_gpu.cpp
)

target_compile_options(kfolcalc PRIVATE -O3 -fopenmp -std=c++17)
# OpenMP 静态链接: 防止运行时缺 libomp.so 闪退 (APK 需自包含)
target_link_options(kfolcalc PRIVATE -fopenmp -static-openmp -Wl,-z,max-page-size=16384)
target_link_libraries(kfolcalc PRIVATE log android)

# OpenCL: NDK 环境可能没有 OpenCL 头/库, 找不到就用 dlopen 运行时探测
find_package(OpenCL QUIET)
if(OpenCL_FOUND)
    target_link_libraries(kfolcalc PRIVATE OpenCL::OpenCL)
    target_compile_definitions(kfolcalc PRIVATE HAVE_OPENCL=1)
endif()