# Third-Party Notices

'dans-vk' keeps third-party code under 'vendor/'. That directory is gitignored
rather than committed, so the exact upstream URLs, tags, and commits in
'vendor/versions.md' (and the table below) are what define the expected tree.

The vendored trees are intentionally shortened copies. The removed material is
limited to non-runtime material such as tests, examples, generated docs, CI
metadata, IDE project files, and unused helper backends. Core source, public
headers, build integration needed by 'dans-vk', and license notices are kept.

Vulkan SDK components, Vulkan loader/headers/libraries, and 'glslc' are not
vendored. They are toolchain/system dependencies.

## Vendored Dependencies

| Dependency | Location | Upstream pin | License notice |
| --- | --- | --- | --- |
| Dear ImGui | 'vendor/imgui' | 'ocornut/imgui' 'docking' commit 'ed9d1e742793f7e4333565f891b4e3821b205f09' | MIT, see 'vendor/imgui/LICENSE.txt' |
| SDL | 'vendor/SDL' | 'libsdl-org/SDL' tag 'release-3.2.30', commit 'f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc' | zlib-style, see 'vendor/SDL/LICENSE.txt' |
| GLM | 'vendor/glm' | 'g-truc/glm' tag '1.0.3', commit '8d1fd52e5ab5590e2c81768ace50c72bae28f2ed' | MIT or Happy Bunny License, see 'vendor/glm/copying.txt' |
| Vulkan Memory Allocator | 'vendor/vma' | 'GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator' tag 'v3.3.0', commit '1d8f600fd424278486eade7ed3e877c99f0846b1' | MIT, see 'vendor/vma/LICENSE.txt' |
| nlohmann/json | 'vendor/nlohmann' | 'nlohmann/json' tag 'v3.11.3', commit '9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03' | MIT, see 'vendor/nlohmann/LICENSE.MIT' |
| stb | 'vendor/stb' | 'nothings/stb' 'master' commit '31c1ad37456438565541f4919958214b6e762fb4' | Public domain or MIT, see license text at the bottom of each vendored header |
| Inter font | 'vendor/fonts' | 'rsms/inter' tag 'v4.1', commit 'e3a3d4c57d5ecc01453a575621882a384c1995a3' | SIL Open Font License 1.1, see 'vendor/fonts/LICENSE.txt' |

## Shortened Copies

- Dear ImGui was shortened to the core sources and the SDL3/Vulkan backends.
- SDL was shortened to the source, public headers, CMake files, Wayland
  protocol data, and license/project metadata needed for the CMake build.
- GLM was shortened to headers, CMake integration, readme, and license.
- Vulkan Memory Allocator was shortened to 'vk_mem_alloc.h' plus its license.
- nlohmann/json was shortened to the single-header distribution plus its
  license.
- stb was shortened to 'stb_image.h', 'stb_image_write.h', and
  'stb_truetype.h'.
- Inter was shortened to a single 'Inter-Regular.ttf' weight plus the
  upstream OFL-1.1 license text.
