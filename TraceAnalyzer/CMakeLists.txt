file(GLOB TA_SOURCES "*.cpp")
file(GLOB TA_HEADERS "*.h")

add_library(TraceAnalyzer STATIC ${TA_SOURCES} ${TA_HEADERS})
target_include_directories(TraceAnalyzer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(TraceAnalyzer PRIVATE /utf-8)
endif()
