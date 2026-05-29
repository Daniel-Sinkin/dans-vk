cmake_minimum_required(VERSION 3.20)

if(APPLE AND NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
  foreach(DANS_VK_LLVM_PREFIX IN ITEMS /opt/homebrew/opt/llvm /usr/local/opt/llvm)
    if(EXISTS "${DANS_VK_LLVM_PREFIX}/bin/clang" AND EXISTS "${DANS_VK_LLVM_PREFIX}/bin/clang++")
      set(CMAKE_C_COMPILER "${DANS_VK_LLVM_PREFIX}/bin/clang" CACHE FILEPATH "C compiler" FORCE)
      set(CMAKE_CXX_COMPILER "${DANS_VK_LLVM_PREFIX}/bin/clang++" CACHE FILEPATH "C++ compiler" FORCE)
      message(STATUS "Using LLVM toolchain from ${DANS_VK_LLVM_PREFIX}")
      break()
    endif()
  endforeach()
endif()

project(dans_vk VERSION 0.2.0 LANGUAGES C CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(DANS_VK_IS_TOP_LEVEL ON)
else()
  set(DANS_VK_IS_TOP_LEVEL OFF)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DANS_VK_BUILD_RUNTIME "Build the Vulkan runtime target" ON)
option(DANS_VK_BUILD_EXAMPLES "Build dans_vk example applications" ${DANS_VK_IS_TOP_LEVEL})
option(DANS_VK_BUILD_TESTS "Build dans_vk tests" ${DANS_VK_IS_TOP_LEVEL})
option(DANS_VK_USE_SYSTEM_VULKAN "Build Vulkan runtime if Vulkan is discoverable" ON)
option(DANS_VK_ENABLE_PCH "Build Vulkan runtime targets with precompiled headers" ON)

add_library(dans_vk_warnings INTERFACE)
target_compile_options(dans_vk_warnings INTERFACE
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

if(NOT TARGET glm::glm)
  add_subdirectory(external/glm EXCLUDE_FROM_ALL)
endif()

set(DANS_VK_CORE_SOURCES
  dans/vk/assets.cpp
  dans/vk/camera.cpp
  dans/vk/geometry.cpp
  dans/vk/mesh.cpp
)

add_library(dans_vk_core STATIC ${DANS_VK_CORE_SOURCES})
add_library(dans::vk::core ALIAS dans_vk_core)
target_include_directories(dans_vk_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_include_directories(dans_vk_core SYSTEM PRIVATE
  external
)
target_link_libraries(dans_vk_core PUBLIC glm::glm)
target_link_libraries(dans_vk_core PRIVATE dans_vk_warnings)

add_library(dans_vk_picker STATIC dans/vk/plugins/picker.cpp)
add_library(dans::vk::picker ALIAS dans_vk_picker)
target_include_directories(dans_vk_picker PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(dans_vk_picker PUBLIC dans_vk_core)
target_link_libraries(dans_vk_picker PRIVATE dans_vk_warnings)

add_library(dans_vk_viz STATIC dans/vk/plugins/viz.cpp)
add_library(dans::vk::viz ALIAS dans_vk_viz)
target_include_directories(dans_vk_viz PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(dans_vk_viz PUBLIC dans_vk_core)
target_link_libraries(dans_vk_viz PRIVATE dans_vk_warnings)

add_library(dans_vk_manipulator STATIC dans/vk/plugins/manipulator.cpp)
add_library(dans::vk::manipulator ALIAS dans_vk_manipulator)
target_include_directories(dans_vk_manipulator PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(dans_vk_manipulator PUBLIC dans_vk_core)
target_link_libraries(dans_vk_manipulator PRIVATE dans_vk_warnings)

function(dans_vk_enable_pch target_name)
  if(DANS_VK_ENABLE_PCH AND TARGET ${target_name})
    target_precompile_headers(${target_name} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/dans/vk/pch.hpp
    )
  endif()
endfunction()

if(DANS_VK_BUILD_TESTS)
  enable_testing()
  add_executable(dans_vk_tests tests/test_main.cpp)
  target_link_libraries(dans_vk_tests PRIVATE dans_vk_picker dans_vk_viz dans_vk_manipulator dans_vk_warnings)
  add_test(NAME dans_vk_tests COMMAND dans_vk_tests)
endif()

if(DANS_VK_BUILD_RUNTIME AND DANS_VK_USE_SYSTEM_VULKAN)
  find_package(Vulkan QUIET)
endif()

if(DANS_VK_BUILD_RUNTIME AND Vulkan_FOUND)
  find_program(DANS_VK_GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin")
  if(NOT DANS_VK_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Vulkan was found, but glslc was not. Set VULKAN_SDK or install Vulkan shader tools.")
  endif()

  set(DANS_VK_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  set(DANS_VK_SHADER_SOURCES
    dans/vk/shaders/environment.vert
    dans/vk/shaders/environment.frag
    dans/vk/shaders/mesh.vert
    dans/vk/shaders/mesh_position_normal.vert
    dans/vk/shaders/mesh_quantized_position_normal.vert
    dans/vk/shaders/mesh.frag
    dans/vk/shaders/shadow.vert
    dans/vk/shaders/shadow_quantized_position.vert
    dans/vk/shaders/debug_line.vert
    dans/vk/shaders/debug_line.frag
    dans/vk/shaders/text_2d.vert
    dans/vk/shaders/text_2d.frag
    dans/vk/shaders/shape_2d.vert
    dans/vk/shaders/shape_2d.frag
  )
  set(DANS_VK_SHADER_OUTPUTS)
  foreach(DANS_VK_SHADER ${DANS_VK_SHADER_SOURCES})
    get_filename_component(DANS_VK_SHADER_NAME "${DANS_VK_SHADER}" NAME)
    set(DANS_VK_SHADER_OUTPUT "${DANS_VK_SHADER_OUTPUT_DIR}/${DANS_VK_SHADER_NAME}.spv")
    add_custom_command(
      OUTPUT "${DANS_VK_SHADER_OUTPUT}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${DANS_VK_SHADER_OUTPUT_DIR}"
      COMMAND "${DANS_VK_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${DANS_VK_SHADER}" -o "${DANS_VK_SHADER_OUTPUT}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${DANS_VK_SHADER}"
      VERBATIM
    )
    list(APPEND DANS_VK_SHADER_OUTPUTS "${DANS_VK_SHADER_OUTPUT}")
  endforeach()
  add_custom_target(dans_vk_shaders DEPENDS ${DANS_VK_SHADER_OUTPUTS})

  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
  set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
  if(NOT TARGET SDL3::SDL3)
    add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
  endif()

  add_library(dans_vk_imgui_vulkan STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_vulkan.cpp
  )
  target_include_directories(dans_vk_imgui_vulkan SYSTEM PUBLIC
    external/imgui
    external/imgui/backends
  )
  target_link_libraries(dans_vk_imgui_vulkan PUBLIC SDL3::SDL3 Vulkan::Vulkan)

  add_library(dans_vk_vma STATIC dans/vk/vma_support.cpp)
  target_include_directories(dans_vk_vma SYSTEM PUBLIC external/vma/include)
  target_link_libraries(dans_vk_vma PUBLIC Vulkan::Vulkan)

  add_library(dans_vk STATIC
    dans/vk/assets_write.cpp
    dans/vk/font_atlas.cpp
    dans/vk/runtime.cpp
    dans/vk/stb_image_impl.cpp
    dans/vk/stb_image_write_impl.cpp
    dans/vk/stb_truetype_impl.cpp
  )
  add_library(dans::vk ALIAS dans_vk)
  add_library(dans::vk::runtime ALIAS dans_vk)
  set_source_files_properties(
    dans/vk/stb_image_impl.cpp
    dans/vk/stb_image_write_impl.cpp
    dans/vk/stb_truetype_impl.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
  )
  add_dependencies(dans_vk dans_vk_shaders)
  target_include_directories(dans_vk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )
  target_include_directories(dans_vk SYSTEM PRIVATE
    external
    external/stb
  )
  target_compile_definitions(dans_vk PRIVATE
    DANS_VK_SHADER_DIR="${DANS_VK_SHADER_OUTPUT_DIR}"
  )
  target_link_libraries(dans_vk PUBLIC
    dans_vk_core
    dans_vk_imgui_vulkan
    dans_vk_vma
    SDL3::SDL3
    Vulkan::Vulkan
  )
  target_link_libraries(dans_vk PRIVATE dans_vk_warnings)
  dans_vk_enable_pch(dans_vk)

  if(DANS_VK_BUILD_EXAMPLES)
    add_executable(dans_vk_basic_app app/main.cpp)
    target_link_libraries(dans_vk_basic_app PRIVATE dans_vk dans_vk_viz dans_vk_warnings)
    target_compile_definitions(dans_vk_basic_app PRIVATE
      DANS_VK_DEFAULT_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/external/fonts/Inter-Regular.ttf"
    )
    dans_vk_enable_pch(dans_vk_basic_app)

    add_executable(dans_vk_2d_app app/main_2d.cpp)
    target_link_libraries(dans_vk_2d_app PRIVATE dans_vk dans_vk_warnings)
    target_include_directories(dans_vk_2d_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(dans_vk_2d_app SYSTEM PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/external
      ${CMAKE_CURRENT_SOURCE_DIR}/app/viz/external
    )
    target_compile_definitions(dans_vk_2d_app PRIVATE
      DANS_VK_DEFAULT_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/external/fonts/Inter-Regular.ttf"
    )
    dans_vk_enable_pch(dans_vk_2d_app)
  endif()

  if(DANS_VK_BUILD_TESTS)
    add_executable(dans_vk_runtime_tests tests/test_runtime.cpp)
    target_link_libraries(dans_vk_runtime_tests PRIVATE dans_vk dans_vk_warnings)
    dans_vk_enable_pch(dans_vk_runtime_tests)
    add_test(NAME dans_vk_runtime_tests COMMAND dans_vk_runtime_tests)
  endif()
elseif(DANS_VK_BUILD_RUNTIME)
  if(DANS_VK_IS_TOP_LEVEL OR DANS_VK_BUILD_EXAMPLES)
    message(WARNING "Vulkan runtime was requested but Vulkan was not found; building core targets only.")
  endif()
endif()
