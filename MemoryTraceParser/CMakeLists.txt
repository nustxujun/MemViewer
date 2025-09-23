cmake_minimum_required(VERSION 3.10)

project(MemoryTraceParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES
    "*.cpp"
    "*.h"
)

add_executable(MemoryTraceParser ${SOURCES})