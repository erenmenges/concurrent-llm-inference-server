cmake_minimum_required(VERSION 3.24)
project(infserve LANGUAGES CXX)

include(FetchContent)
find_package(Threads REQUIRED)

set(LLAMA_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  llama_cpp
  GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
  GIT_TAG        b10741
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(llama_cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_library(infserve_core src/engine.cpp src/llama_model.cpp)
target_link_libraries(infserve_core PUBLIC llama Threads::Threads)
target_include_directories(infserve_core PUBLIC src)
target_compile_options(infserve_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE infserve_core)
target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(infserve_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(infserve_core PUBLIC -fsanitize=address,undefined)
endif()