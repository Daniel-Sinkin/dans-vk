# dans-vk

Personal Vulkan helper/runtime for C++ visualization experiments.

This is not a game engine or a cross-API abstraction. It is a small Vulkan runtime
and helper library intended to be pulled directly into project repos, usually as a
Git submodule under `external/dans-vk`.

## Use As A Dependency

```sh
git submodule add git@github.com:Daniel-Sinkin/dans-vk.git external/dans-vk
```

```cmake
add_subdirectory(external/dans-vk)
add_executable(my_app app/main.cpp)
target_link_libraries(my_app PRIVATE dans::vk dans::vk::viz dans::vk::picker)
```

When this repo is included with `add_subdirectory`, examples and tests are off by
default. The reusable targets are:

- `dans_vk_core` / `dans::vk::core` for CPU-side math, geometry, mesh, camera, glTF helpers
- `dans_vk` / `dans::vk` / `dans::vk::runtime` for the Vulkan runtime
- `dans_vk_picker` / `dans::vk::picker`, `dans_vk_viz` / `dans::vk::viz`, and
  `dans_vk_manipulator` / `dans::vk::manipulator` for optional helper plugins

The Vulkan runtime still expects system Vulkan SDK support, including `glslc`.

## Local Assets

Redistribution-sensitive or bulky assets belong in `local/assets/`. That directory
is intentionally ignored by Git. The framework does not require checked-in assets
to build; project repos can keep their own ignored asset cache and pass explicit
paths to loaders or runtime configuration.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

If Vulkan and `glslc` are available, the top-level build also produces
`dans_vk_basic_app`:

```sh
./run.sh
./run.sh --smoke-frames 20 --hide-ui --screenshot run/basic.png
```
