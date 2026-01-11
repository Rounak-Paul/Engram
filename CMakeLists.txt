cmake_minimum_required(VERSION 3.15)
project(engram VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENGRAM_BUILD_TESTS "Build test suite" ON)
option(ENGRAM_BUILD_EXAMPLES "Build example programs" ON)
option(ENGRAM_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENGRAM_ENABLE_VULKAN "Enable Vulkan compute" ON)

if(ENGRAM_ENABLE_SIMD)
    add_compile_definitions(ENGRAM_SIMD_ENABLED)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=native)
    endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -ffast-math)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /fp:fast)
    endif()
endif()

set(ENGRAM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(ENGRAM_SOURCES
    src/core/engram.c
    src/core/substrate.c
    src/core/synapse.c
    src/core/pathway.c
    src/regions/hippocampus.c
    src/regions/cortex.c
    src/regions/wernicke.c
    src/processes/propagate.c
    src/system/persistence.c
    src/util/hash.c
    src/util/random.c
)

if(WIN32)
    list(APPEND ENGRAM_SOURCES
        src/platform/thread_win32.c
    )
else()
    list(APPEND ENGRAM_SOURCES
        src/platform/thread_posix.c
    )
endif()

add_library(engram STATIC ${ENGRAM_SOURCES})
target_include_directories(engram PUBLIC ${ENGRAM_INCLUDE_DIR})
target_include_directories(engram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(UNIX AND NOT APPLE)
    target_link_libraries(engram PRIVATE pthread m)
elseif(APPLE)
    target_link_libraries(engram PRIVATE pthread m)
elseif(WIN32)
    target_link_libraries(engram PRIVATE synchronization)
endif()

if(ENGRAM_ENABLE_VULKAN)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        target_compile_definitions(engram PRIVATE ENGRAM_VULKAN_ENABLED)
        target_link_libraries(engram PRIVATE Vulkan::Vulkan)
        target_sources(engram PRIVATE
            src/compute/vulkan_compute.c
        )
    else()
        message(STATUS "Vulkan not found, building without GPU acceleration")
    endif()
endif()

if(ENGRAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ENGRAM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS engram
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/engram DESTINATION include)
