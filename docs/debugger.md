# Debugger backend (`dans::dbg`)

`dans_dbg` is an LLDB-based debugger backend meant to be driven by a frontend
(the Vulkan/ImGui runtime). It wraps the LLDB SB API behind a PIMPL
(`dans/dbg/debugger.hpp`) so the frontend never includes lldb headers and never
links liblldb directly.

## Same behavior on every platform

The guarantee that the debugger behaves identically on macOS and Windows comes
from one fact: **both platforms compile the same backend source
(`dans/dbg/debugger.cpp`) against the same LLDB SB API.** Breakpoints, launching,
stepping, variable reads, backtraces, and expression evaluation are all SB API
calls, so they behave the same regardless of OS. What differs per platform is
only build/runtime plumbing that is invisible to debugger behavior:

| | macOS | Windows |
|---|---|---|
| liblldb | Homebrew `/opt/homebrew/opt/llvm` | LLVM installer `C:/Program Files/LLVM` |
| SB API headers | already in the Homebrew prefix | fetched into `extern/` by `scripts/setup_lldb.ps1` |
| launch mechanism | Apple's signed `debugserver` (auto-found) | native LLDB process plugin |
| runtime deps | none to copy | `liblldb.dll` + `python3xx.dll` copied next to the exe |

Keep both sides on the same LLVM major version (currently 22) so the SB API is
identical.

## macOS: how debugging actually works (Apple Silicon)

The privileged operation a debugger needs is `task_for_pid()` on the target. On
macOS that work is not done by the debugger process. liblldb spawns a separate
helper, **debugserver**, which holds the privilege:

    /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Resources/debugserver

Apple signs it with the private `com.apple.private.cs.debugger` entitlement.
Homebrew LLVM ships **no** debugserver, but with Xcode selected (`xcode-select
-p`) liblldb auto-discovers Apple's. So debugging locally built code needs **no
code signing, no entitlements, no sudo, and developer mode off**. The backend
also sets `LLDB_DEBUGSERVER_PATH` to Apple's debugserver as a fallback if it is
unset; a wrong path is harmless (liblldb ignores a non-existent one).

Still optional on macOS:

- **Attaching to an already-running process** goes through `taskgated`; enable
  developer mode once so it does not prompt:
  `scripts/setup_macos_debugger.sh --enable`.
- **Bundling your own debugserver** instead of Apple's requires signing it with
  the debugger entitlement (`dans/dbg/debugger.entitlements`). Not needed here.
- **SIP** stays on; it only blocks debugging Apple/system binaries, not your own
  code.

## Windows: setup

Fetch the SB API headers (matched to your local LLVM) and let CMake find the
installer's liblldb:

    pwsh scripts/setup_lldb.ps1

Adjust the `DANS_VK_LLVM_ROOT` / `DANS_VK_PYTHON_DLL` cache vars if your LLVM or
Python live elsewhere. The build copies `liblldb.dll` and the Python DLL it
imports next to `dans_vk_debugger`.

## Build

Gated on `DANS_VK_BUILD_DEBUGGER` (ON at top level). Targets:

- `dans_dbg` - the backend library (`dans::dbg`).
- `dans_vk_debugger` - a headless driver / smoke harness (`app/debugger/main.cpp`).
- `dans_vk_debugger_sample` - a tiny `-g -O0` program to debug
  (`app/debugger/sample/hello.cpp`).

## Smoke test

    cmake --build build --target dans_vk_debugger dans_vk_debugger_sample
    ./build/dans_vk_debugger ./build/dans_vk_debugger_sample --break-fn add

Breaks in `add` (scoped to the executable, so it does not also match a system
`add`), prints the backtrace and locals at each hit, continues to exit, and
reports the exit status. `--break hello.cpp:5` breaks by file:line; `--steps N`
single-steps N times before continuing; `-- args...` passes arguments to the
sample.

## API shape

```cpp
dans::dbg::Debugger dbg;
dbg.load_executable("./build/dans_vk_debugger_sample");
dbg.set_breakpoint_by_name("add");          // or set_breakpoint("hello.cpp", 5)
auto stop = dbg.launch();
while (dans::dbg::stopped_for_inspection(stop.state)) {
    for (const auto& v : dbg.locals()) { /* feed an ImGui table */ }
    stop = dbg.cont();                       // or step_over()/step_into()/step_out()
}
```

`launch`/`cont`/`step_*` block until the inferior next stops or exits and return
a `StopInfo`. Everything else (`backtrace`, `locals`, `evaluate`, `state`) is a
query against the current stop. No LLDB type appears in the public header, so a
frontend links `dans::dbg` without taking a compile dependency on liblldb.
