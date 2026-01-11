cmake_minimum_required(VERSION 3.15)
project(engram VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENGRAM_BUILD_TESTS "Build test suite" ON)
option(ENGRAM_BUILD_EXAMPLES "Build example programs" ON)
option(ENGRAM_ENABLE_SIMD "Enable SIMD optimizations" ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(ENGRAM_ENABLE_SIMD)
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        check_c_compiler_flag("-msse4.1" COMPILER_SUPPORTS_SSE41)
        if(COMPILER_SUPPORTS_AVX2)
            add_compile_options(-mavx2)
            add_compile_definitions(ENGRAM_SIMD_AVX2)
        elseif(COMPILER_SUPPORTS_SSE41)
            add_compile_options(-msse4.1)
            add_compile_definitions(ENGRAM_SIMD_SSE41)
        endif()
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX)
    if(ENGRAM_ENABLE_SIMD)
        add_compile_options(/arch:AVX2)
        add_compile_definitions(ENGRAM_SIMD_AVX2)
    endif()
endif()

set(ENGRAM_SOURCES
    src/core/engram.c
    src/core/allocator.c
    src/core/neuron.c
    src/core/synapse.c
    src/core/cluster.c
    src/core/pathway.c
    src/regions/brainstem.c
    src/regions/thalamus.c
    src/regions/hippocampus.c
    src/regions/amygdala.c
    src/regions/neocortex.c
    src/processes/encoding.c
    src/processes/plasticity.c
    src/processes/decay.c
    src/processes/consolidation.c
    src/processes/replay.c
    src/system/clock.c
    src/system/arousal.c
    src/system/governor.c
    src/system/persistence.c
)

if(WIN32)
    list(APPEND ENGRAM_SOURCES
        src/platform/thread_win32.c
        src/platform/time_win32.c
    )
else()
    list(APPEND ENGRAM_SOURCES
        src/platform/thread_posix.c
        src/platform/time_posix.c
    )
endif()

add_library(engram ${ENGRAM_SOURCES})

target_include_directories(engram
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(engram PRIVATE Threads::Threads m)
elseif(APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(engram PRIVATE Threads::Threads)
endif()

if(ENGRAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ENGRAM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS engram
    EXPORT engramTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/engram
    DESTINATION include
)

install(EXPORT engramTargets
    FILE engramTargets.cmake
    NAMESPACE engram::
    DESTINATION lib/cmake/engram
)
