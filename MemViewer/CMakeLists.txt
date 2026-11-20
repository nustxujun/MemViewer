cmake_minimum_required(VERSION 3.10)

project(MemViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# TraceAnalyzer static library (sibling directory)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../TraceAnalyzer ${CMAKE_CURRENT_BINARY_DIR}/TraceAnalyzer)

include_directories(imgui)

file(GLOB SOURCES
    "*.cpp"
    "*.h"
    "MemViewer.rc"
    "NatvisFile.natvis"
)

file(GLOB IMGUI_SOURCES
    "imgui/*.h"
    "imgui/*.cpp"
    "imgui/backends/imgui_impl_dx12.*"
    "imgui/backends/imgui_impl_win32.*"
)

source_group("imgui" FILES ${IMGUI_SOURCES})
source_group("src" FILES ${SOURCES})

add_executable(MemViewer ${SOURCES} ${IMGUI_SOURCES})
target_link_libraries(MemViewer PRIVATE TraceAnalyzer)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(MemViewer PRIVATE d3d12.lib d3dcompiler.lib dxgi.lib)
    target_link_options(MemViewer PRIVATE "/STACK:16777216")
endif()
