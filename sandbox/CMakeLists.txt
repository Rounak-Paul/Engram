cmake_minimum_required(VERSION 3.15)
project(engram_sandbox VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(sandbox main.c)
target_link_libraries(sandbox PRIVATE engram)
