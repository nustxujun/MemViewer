cmake_minimum_required(VERSION 3.18)
project(MemAnalyzerMCP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../TraceAnalyzer ${CMAKE_CURRENT_BINARY_DIR}/TraceAnalyzer)
add_subdirectory(exporter)

find_package(Python3 COMPONENTS Interpreter Development QUIET)
if(Python3_FOUND)
    add_subdirectory(bindings)
else()
    message(WARNING "Python3 not found, skipping pybind11 bindings")
endif()
