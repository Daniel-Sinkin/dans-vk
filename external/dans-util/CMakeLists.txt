cmake_minimum_required(VERSION 3.20)

if(APPLE AND NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
    foreach(DANS_UTIL_LLVM_PREFIX IN ITEMS /opt/homebrew/opt/llvm /usr/local/opt/llvm)
        if(EXISTS "${DANS_UTIL_LLVM_PREFIX}/bin/clang" AND EXISTS "${DANS_UTIL_LLVM_PREFIX}/bin/clang++")
            set(CMAKE_C_COMPILER "${DANS_UTIL_LLVM_PREFIX}/bin/clang" CACHE FILEPATH "C compiler" FORCE)
            set(CMAKE_CXX_COMPILER "${DANS_UTIL_LLVM_PREFIX}/bin/clang++" CACHE FILEPATH "C++ compiler" FORCE)
            message(STATUS "Using LLVM toolchain from ${DANS_UTIL_LLVM_PREFIX}")
            break()
        endif()
    endforeach()
endif()

project(dans_util LANGUAGES C CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_library(dans_util_warnings INTERFACE)
target_compile_options(dans_util_warnings INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wconversion
        -Wfloat-conversion
        -Wsign-conversion
        -Wcast-qual
        -Wcast-align
        -Wformat=2
        -Wundef
        -Wnull-dereference
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wold-style-cast
        -Wzero-as-null-pointer-constant
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Wreturn-type>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:
        -Wconditional-uninitialized
        -Wextra-semi
        -Wimplicit-int-conversion
        -Wimplicit-int-float-conversion
        -Wnewline-eof
        -Wshorten-64-to-32
        -Wswitch-enum
        -Wunreachable-code>
    $<$<CXX_COMPILER_ID:GNU>:
        -Wduplicated-branches
        -Wduplicated-cond
        -Wlogical-op
        -Wsign-compare
        -Wuseless-cast>
)

add_library(dans_util STATIC
    src/chrono.cpp
    src/dev.cpp
    src/env.cpp
    src/fmt.cpp
    src/random.cpp
    src/str.cpp
    src/util.cpp
)
add_library(dans::util ALIAS dans_util)
target_link_libraries(dans_util PRIVATE
    dans_util_warnings
)
target_include_directories(dans_util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/dans-core/include>
)
target_precompile_headers(dans_util PRIVATE
    <dans/pch.hpp>
)

add_executable(dans_util_scratch
    app/main.cpp
)
target_link_libraries(dans_util_scratch PRIVATE
    dans_util
    dans_util_warnings
)
target_include_directories(dans_util_scratch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dans-core/include
)
target_precompile_headers(dans_util_scratch PRIVATE
    <dans/pch.hpp>
)
