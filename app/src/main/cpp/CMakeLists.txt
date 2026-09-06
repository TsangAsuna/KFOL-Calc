cmake_minimum_required(VERSION 3.22.1)
project(kfolcalc)

find_package(OpenCL)

add_library(kfolcalc SHARED
    jni_bridge.cpp
    kfol_core.cpp
    kfol_cpu.cpp
    kfol_gpu.cpp
)

target_compile_options(kfolcalc PRIVATE -O3 -fopenmp -std=c++17)
target_link_libraries(kfolcalc PRIVATE log android OpenCL::OpenCL)