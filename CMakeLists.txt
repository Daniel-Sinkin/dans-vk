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

# MSVC's CRT headers mark fopen/strcpy/etc. deprecated in favour of their
# non-standard _s variants; under -Werror that breaks the build on Windows.
if(WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Force glm's length_type (vector/matrix operator[] index and .length()) to
# std::size_t so glm indexing matches usize everywhere. Must be global and
# identical for every TU or glm types mismatch across translation units;
# dans/linalg/linalg.hpp static_asserts it held.
add_compile_definitions(GLM_FORCE_SIZE_T_LENGTH)

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
  add_subdirectory(vendor/glm EXCLUDE_FROM_ALL)
endif()

add_library(dans_core INTERFACE)
add_library(dans::core ALIAS dans_core)
target_include_directories(dans_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_library(dans_util STATIC
  dans/dans-util/dans_util.cpp
)
add_library(dans::util ALIAS dans_util)
target_include_directories(dans_util PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_util PUBLIC dans::core)
target_link_libraries(dans_util PRIVATE dans_vk_warnings)

add_library(dans_linalg INTERFACE)
add_library(dans::linalg ALIAS dans_linalg)
target_include_directories(dans_linalg INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_linalg INTERFACE dans::core glm::glm)

add_library(dans_gfx INTERFACE)
add_library(dans::gfx ALIAS dans_gfx)
target_include_directories(dans_gfx INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_gfx INTERFACE dans::core dans_linalg glm::glm)

set(DANS_VK_CORE_SOURCES
  dans/vk/drawlist.cpp
)

add_library(dans_vk_core STATIC ${DANS_VK_CORE_SOURCES})
add_library(dans::vk::core ALIAS dans_vk_core)
target_include_directories(dans_vk_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_include_directories(dans_vk_core SYSTEM PRIVATE
  vendor
)
target_link_libraries(dans_vk_core PUBLIC glm::glm dans::core dans_gfx dans_linalg dans_geom dans_camera dans_mesh)
target_link_libraries(dans_vk_core PRIVATE dans_vk_warnings)

add_library(dans_font STATIC
  dans/font/font_atlas.cpp
  dans/font/stb_truetype_impl.cpp
)
add_library(dans::font ALIAS dans_font)
target_include_directories(dans_font PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_include_directories(dans_font SYSTEM PRIVATE vendor/stb)
target_link_libraries(dans_font PUBLIC dans::core)
target_link_libraries(dans_font PRIVATE dans_vk_warnings dans::util)

add_library(dans_geom STATIC dans/geom/geometry.cpp)
add_library(dans::geom ALIAS dans_geom)
target_include_directories(dans_geom PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_geom PUBLIC dans_linalg)
target_link_libraries(dans_geom PRIVATE dans_vk_warnings)

add_library(dans_camera STATIC dans/camera/camera.cpp)
add_library(dans::camera ALIAS dans_camera)
target_include_directories(dans_camera PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_camera PUBLIC dans_geom dans_linalg)
target_link_libraries(dans_camera PRIVATE dans_vk_warnings)

add_library(dans_mesh STATIC dans/mesh/mesh.cpp)
add_library(dans::mesh ALIAS dans_mesh)
target_include_directories(dans_mesh PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(dans_mesh PUBLIC dans_geom dans_gfx dans_linalg)
target_link_libraries(dans_mesh PRIVATE dans_vk_warnings)

add_library(dans_image STATIC
  dans/image/image.cpp
  dans/image/stb_image_impl.cpp
)
add_library(dans::image ALIAS dans_image)
target_include_directories(dans_image PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_include_directories(dans_image SYSTEM PRIVATE vendor/stb)
target_link_libraries(dans_image PUBLIC dans::core dans_gfx)
target_link_libraries(dans_image PRIVATE dans_vk_warnings dans::util)

add_library(dans_mesh_io STATIC
  dans/mesh_io/gltf.cpp
  dans/mesh_io/gltf_write.cpp
)
add_library(dans::mesh_io ALIAS dans_mesh_io)
target_include_directories(dans_mesh_io PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_include_directories(dans_mesh_io SYSTEM PRIVATE vendor vendor/stb)
target_link_libraries(dans_mesh_io PUBLIC dans_mesh dans_gfx dans_linalg)
target_link_libraries(dans_mesh_io PRIVATE dans_image dans_vk_warnings)

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
  target_link_libraries(dans_vk_tests PRIVATE dans_vk_picker dans_vk_viz dans_vk_manipulator dans_mesh_io dans_vk_warnings)
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
    add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
  endif()

  add_library(dans_vk_imgui_vulkan STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_vulkan.cpp
  )
  target_include_directories(dans_vk_imgui_vulkan SYSTEM PUBLIC
    vendor/imgui
    vendor/imgui/backends
  )
  target_link_libraries(dans_vk_imgui_vulkan PUBLIC SDL3::SDL3 Vulkan::Vulkan)

  add_library(dans_vk_vma STATIC dans/vk/vma_support.cpp)
  target_include_directories(dans_vk_vma SYSTEM PUBLIC vendor/vma/include)
  target_link_libraries(dans_vk_vma PUBLIC Vulkan::Vulkan)

  add_library(dans_vk STATIC
    dans/vk/runtime.cpp
  )
  add_library(dans::vk ALIAS dans_vk)
  add_library(dans::vk::runtime ALIAS dans_vk)
  add_dependencies(dans_vk dans_vk_shaders)
  target_include_directories(dans_vk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )
  target_include_directories(dans_vk SYSTEM PRIVATE
    vendor
    vendor/stb
  )
  target_compile_definitions(dans_vk PRIVATE
    DANS_VK_SHADER_DIR="${DANS_VK_SHADER_OUTPUT_DIR}"
  )
  target_link_libraries(dans_vk PUBLIC
    dans_vk_core
    dans_font
    dans_image
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
      DANS_VK_DEFAULT_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/vendor/fonts/Inter-Regular.ttf"
    )
    dans_vk_enable_pch(dans_vk_basic_app)

    # Windows loads SDL3.dll from the executable's own directory, so copy it
    # next to each app after building.
    if(WIN32)
      foreach(DANS_VK_APP dans_vk_basic_app)
        add_custom_command(TARGET ${DANS_VK_APP} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${DANS_VK_APP}>
          VERBATIM)
      endforeach()
    endif()
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

option(DANS_VK_BUILD_DEBUGGER "Build the LLDB-backed debugger app" ${DANS_VK_IS_TOP_LEVEL})

if(DANS_VK_BUILD_DEBUGGER)
  set(DANS_VK_LLVM_ROOT "C:/Program Files/LLVM" CACHE PATH "LLVM install root (for liblldb)")
  set(DANS_VK_LLDB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/llvm-project/lldb/include"
      CACHE PATH "LLDB SB API headers (run scripts/setup_lldb.ps1 to fetch)")
  if(WIN32)
    set(DANS_VK_LLDB_LIB "${DANS_VK_LLVM_ROOT}/lib/liblldb.lib" CACHE FILEPATH "liblldb import library")
    set(DANS_VK_LLDB_DLL "${DANS_VK_LLVM_ROOT}/bin/liblldb.dll" CACHE FILEPATH "liblldb runtime DLL")
    set(DANS_VK_PYTHON_DLL "C:/Users/Daniel/AppData/Local/Programs/Python/Python311/python311.dll"
        CACHE FILEPATH "python DLL that liblldb.dll imports")
  else()
    set(DANS_VK_LLDB_LIB "lldb" CACHE STRING "liblldb link name/path")
  endif()

  if(EXISTS "${DANS_VK_LLDB_INCLUDE_DIR}/lldb/API/LLDB.h")
    add_executable(dans_vk_debugger app/debugger/main.cpp)
    target_include_directories(dans_vk_debugger SYSTEM PRIVATE "${DANS_VK_LLDB_INCLUDE_DIR}")
    target_link_libraries(dans_vk_debugger PRIVATE "${DANS_VK_LLDB_LIB}" dans_vk_warnings)

    add_executable(dans_vk_debugger_sample app/debugger/sample/hello.cpp)
    target_compile_options(dans_vk_debugger_sample PRIVATE -g -O0)

    if(WIN32)
      add_custom_command(TARGET dans_vk_debugger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${DANS_VK_LLDB_DLL}" "$<TARGET_FILE_DIR:dans_vk_debugger>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${DANS_VK_PYTHON_DLL}" "$<TARGET_FILE_DIR:dans_vk_debugger>"
        VERBATIM)
    endif()
  else()
    message(STATUS "dans_vk_debugger skipped: LLDB SB headers missing. Run scripts/setup_lldb.ps1.")
  endif()
endif()
