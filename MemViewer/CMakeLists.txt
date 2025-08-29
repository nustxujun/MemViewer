cmake_minimum_required(VERSION 3.10)

project(MemViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

# 添加可执行文件
add_executable(MemViewer ${SOURCES} ${IMGUI_SOURCES})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(MemViewer d3d12.lib d3dcompiler.lib dxgi.lib)

endif()