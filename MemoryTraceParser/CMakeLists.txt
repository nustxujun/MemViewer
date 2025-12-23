cmake_minimum_required(VERSION 3.10)


if(APPLE)
    if(NOT CMAKE_CXX_COMPILER)
        set(_GCC_PATH_HINTS
            /opt/homebrew/bin     # Apple Silicon
            /usr/local/bin        # Intel 或老环境
        )

        set(_GCC_MIN_VERSION 11)
        set(_FOUND_GXX "")

        foreach(ver 15 14 13 12 11)
            find_program(_GXX_CANDIDATE
                NAMES g++-${ver}
                PATHS ${_GCC_PATH_HINTS}
                NO_DEFAULT_PATH
            )
            if(_GXX_CANDIDATE)
                execute_process(
                    COMMAND "${_GXX_CANDIDATE}" -dumpversion
                    OUTPUT_VARIABLE _GXX_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )

                # 简单版本判断：只要主版本 >= 11 即可
                if(_GXX_VERSION VERSION_GREATER_EQUAL "${_GCC_MIN_VERSION}")
                    set(_FOUND_GXX "${_GXX_CANDIDATE}")
                    message(STATUS "Found suitable GCC: ${_FOUND_GXX} (version ${_GXX_VERSION})")
                    break()
                else()
                    message(STATUS "Found ${_GXX_CANDIDATE} but version ${_GXX_VERSION} < ${_GCC_MIN_VERSION}, skipping")
                endif()
            endif()
        endforeach()

        if(_FOUND_GXX)
            # 设置为当前项目的 C++ 编译器
            set(CMAKE_CXX_COMPILER "${_FOUND_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        else()
            message(FATAL_ERROR
                "Failed to find GCC >= 11 on macOS.\n"
            )
        endif()
    else()
        message(STATUS "User-specified CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}, skip auto GCC search.")
    endif()
endif()


project(MemoryTraceParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES
    "*.cpp"
    "*.h"
)

add_executable(MemoryTraceParser ${SOURCES})