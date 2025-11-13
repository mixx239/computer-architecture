cmake_minimum_required(VERSION 3.16)
project(labwork4 VERSION 0.1.0 LANGUAGES C CXX)

SET(CMAKE_CXX_STANDARD 23)

find_package(OpenMP REQUIRED)

add_executable(OptimizationLab main.cpp)

target_link_libraries(OptimizationLab PRIVATE OpenMP::OpenMP_CXX)