cmake_minimum_required(VERSION 3.15)
project(engram VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENGRAM_ENABLE_VULKAN "Enable Vulkan compute" ON)

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
    src/engram.c
    src/core/substrate.c
    src/core/propagate.c
    src/core/hnsw.c
    src/regions/wernicke.c
    src/regions/hippocampus.c
    src/regions/cortex.c
    src/util/vector.c
    src/util/hash.c
)

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
    find_package(Vulkan QUIET)
    
    if(NOT Vulkan_FOUND)
        if(DEFINED ENV{VULKAN_SDK})
            set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
        elseif(WIN32)
            file(GLOB VULKAN_SDK_PATHS "C:/VulkanSDK/*")
            if(VULKAN_SDK_PATHS)
                list(GET VULKAN_SDK_PATHS -1 VULKAN_SDK_PATH)
            endif()
        elseif(APPLE)
            set(VULKAN_SDK_SEARCH_PATHS
                "$ENV{HOME}/VulkanSDK"
                "/usr/local"
                "/opt/homebrew"
            )
            foreach(SEARCH_PATH ${VULKAN_SDK_SEARCH_PATHS})
                if(EXISTS "${SEARCH_PATH}/include/vulkan/vulkan.h")
                    set(VULKAN_SDK_PATH ${SEARCH_PATH})
                    break()
                endif()
                file(GLOB VULKAN_VERSIONS "${SEARCH_PATH}/*/macOS")
                if(VULKAN_VERSIONS)
                    list(GET VULKAN_VERSIONS -1 VULKAN_SDK_PATH)
                    break()
                endif()
            endforeach()
        endif()
        
        if(VULKAN_SDK_PATH)
            if(WIN32)
                set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include")
                if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib/vulkan-1.lib")
                else()
                    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib32/vulkan-1.lib")
                endif()
            elseif(APPLE)
                set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/include")
                set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/lib/libvulkan.dylib")
                if(NOT EXISTS ${Vulkan_LIBRARIES})
                    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/lib/libMoltenVK.dylib")
                endif()
            else()
                set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/include")
                set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/lib/libvulkan.so")
            endif()
            
            if(EXISTS ${Vulkan_INCLUDE_DIRS} AND EXISTS ${Vulkan_LIBRARIES})
                set(Vulkan_FOUND TRUE)
            endif()
        endif()
    endif()
    
    if(NOT Vulkan_FOUND AND UNIX AND NOT APPLE)
        find_path(Vulkan_INCLUDE_DIRS vulkan/vulkan.h
            PATHS /usr/include /usr/local/include
        )
        find_library(Vulkan_LIBRARIES vulkan
            PATHS /usr/lib /usr/lib64 /usr/local/lib
        )
        if(Vulkan_INCLUDE_DIRS AND Vulkan_LIBRARIES)
            set(Vulkan_FOUND TRUE)
        endif()
    endif()
    
    if(Vulkan_FOUND)
        target_compile_definitions(engram PUBLIC ENGRAM_VULKAN_ENABLED)
        target_include_directories(engram PUBLIC ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(engram PUBLIC ${Vulkan_LIBRARIES})
        target_sources(engram PRIVATE src/compute/vulkan_compute.c)
        
        find_library(SHADERC_LIB shaderc_combined shaderc
            PATHS 
                "${VULKAN_SDK_PATH}/lib"
                "/usr/local/lib"
                "/opt/homebrew/lib"
                "$ENV{VULKAN_SDK}/lib"
        )
        if(SHADERC_LIB)
            target_link_libraries(engram PRIVATE ${SHADERC_LIB})
            target_compile_definitions(engram PRIVATE ENGRAM_HAS_SHADERC)
            message(STATUS "Shaderc enabled: ${SHADERC_LIB}")
        endif()
        
        if(APPLE)
            target_link_libraries(engram PRIVATE "-framework Metal" "-framework IOSurface" "-framework QuartzCore" "-lc++")
        endif()
        
        message(STATUS "Vulkan enabled: ${Vulkan_LIBRARIES}")
    else()
        message(STATUS "Vulkan not found, building without GPU acceleration")
    endif()
endif()

add_subdirectory(sandbox)

install(TARGETS engram
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/engram DESTINATION include)
