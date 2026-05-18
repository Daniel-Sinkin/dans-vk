# Third-Party Notices

`ds-vk` vendors third-party code directly under `external/` so the framework
can be consumed from a single fixed Git commit. Exact upstream URLs, tags, and
commits are recorded in `external/versions.md`.

The vendored trees are intentionally shortened copies. The removed material is
limited to non-runtime material such as tests, examples, generated docs, CI
metadata, IDE project files, and unused helper backends. Core source, public
headers, build integration needed by `ds-vk`, and license notices are kept.

Vulkan SDK components, Vulkan loader/headers/libraries, and `glslc` are not
vendored. They are toolchain/system dependencies.

## Vendored Dependencies

| Dependency | Location | Upstream pin | License notice |
| --- | --- | --- | --- |
| Dear ImGui | `external/imgui` | `ocornut/imgui` `docking` commit `ed9d1e742793f7e4333565f891b4e3821b205f09` | MIT, see `external/imgui/LICENSE.txt` |
| SDL | `external/SDL` | `libsdl-org/SDL` tag `release-3.2.30`, commit `f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc` | zlib-style, see `external/SDL/LICENSE.txt` |
| GLM | `external/glm` | `g-truc/glm` tag `1.0.3`, commit `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` | MIT or Happy Bunny License, see `external/glm/copying.txt` |
| Vulkan Memory Allocator | `external/vma` | `GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator` tag `v3.3.0`, commit `1d8f600fd424278486eade7ed3e877c99f0846b1` | MIT, see `external/vma/LICENSE.txt` |
| nlohmann/json | `external/nlohmann` | `nlohmann/json` tag `v3.11.3`, commit `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` | MIT, see `external/nlohmann/LICENSE.MIT` |
| stb | `external/stb` | `nothings/stb` `master` commit `31c1ad37456438565541f4919958214b6e762fb4` | Public domain or MIT, see license text at the bottom of each vendored header |

## Shortened Copies

- Dear ImGui was shortened to the core sources and the SDL3/Vulkan backends.
- SDL was shortened to the source, public headers, CMake files, Wayland
  protocol data, and license/project metadata needed for the CMake build.
- GLM was shortened to headers, CMake integration, readme, and license.
- Vulkan Memory Allocator was shortened to `vk_mem_alloc.h` plus its license.
- nlohmann/json was shortened to the single-header distribution plus its
  license.
- stb was shortened to `stb_image.h` and `stb_image_write.h`.
