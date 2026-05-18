# Vendored Sources

Third-party code is committed directly under `external/` so downstream projects
can build `ds-vk` from a fixed repository commit without submodules or
configure-time downloads. Vulkan itself and shader compiler tooling such as
`glslc` are intentionally not vendored; install those through the Vulkan SDK.

The initial source snapshot was copied from
`/Users/danielsinkin/GitHub_private/SPH-Seminar/dfsph_viewer` on 2026-05-15,
with later additions and cleanup in `ds-vk`.

## Dependencies

- Dear ImGui: `https://github.com/ocornut/imgui`
  - Branch: `docking`
  - Commit: `ed9d1e742793f7e4333565f891b4e3821b205f09`
  - Version macro: `1.92.8 WIP` / `19275`
  - Vendored form: shortened; deleted documentation, examples, misc tooling,
    CI metadata, and backends other than SDL3/Vulkan.
- SDL: `https://github.com/libsdl-org/SDL`
  - Tag: `release-3.2.30`
  - Commit: `f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc`
  - Vendored form: shortened; deleted tests, examples, generated docs, CI
    metadata, Android project scaffolding, and Visual Studio/Xcode project
    packages. Core source, public headers, CMake integration, license, and
    basic project metadata are kept.
- GLM: `https://github.com/g-truc/glm`
  - Tag: `1.0.3`
  - Commit: `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`
  - Vendored form: shortened; deleted generated docs, tests, CI metadata,
    debugger/IDE utility files, and the standalone manual. Core headers,
    CMake integration, readme, and license are kept.
- Vulkan Memory Allocator: `https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator`
  - Tag: `v3.3.0`
  - Commit: `1d8f600fd424278486eade7ed3e877c99f0846b1`
  - Vendored form: shortened to `include/vk_mem_alloc.h` plus license notice.
- nlohmann/json: `https://github.com/nlohmann/json`
  - Tag: `v3.11.3`
  - Commit: `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`
  - Vendored form: shortened to the single-header distribution
    `single_include/nlohmann/json.hpp` plus license notice.
- stb: `https://github.com/nothings/stb`
  - Branch: `master`
  - Commit: `31c1ad37456438565541f4919958214b6e762fb4`
  - Vendored files: `stb_image.h` v2.30 and `stb_image_write.h` v1.16
  - Vendored form: shortened to the two single-header libraries used by
    `ds-vk`; the rest of the stb repository is not included.
