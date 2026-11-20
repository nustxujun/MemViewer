include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.12.0
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(TraceAnalyzer_py bindings.cpp)
target_link_libraries(TraceAnalyzer_py PRIVATE TraceAnalyzer)
