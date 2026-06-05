#include "dans/dbg/debugger.hpp"

#include <lldb/API/LLDB.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace dans::dbg
{
namespace
{
int g_init_refcount = 0;

// macOS only. liblldb spawns debugserver to do the privileged task_for_pid
// work; Homebrew LLVM ships none, but with Xcode/Command Line Tools selected
// liblldb auto-discovers Apple's signed-and-entitled debugserver. This only
// fills LLDB_DEBUGSERVER_PATH as a fallback when it is unset and a known Apple
// debugserver exists, so the wrapper keeps working even if xcrun discovery ever
// fails. A wrong path is harmless: liblldb ignores a non-existent
// LLDB_DEBUGSERVER_PATH and falls back. On Windows/Linux liblldb debugs through
// its native process plugin and there is nothing to do here.
auto ensure_debugserver_env() -> void
{
#if defined(__APPLE__)
    if (std::getenv("LLDB_DEBUGSERVER_PATH") != nullptr)
    {
        return;
    }
    static constexpr std::array<std::string_view, 2> candidates{
        "/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Resources/debugserver",
        "/Library/Developer/CommandLineTools/Library/PrivateFrameworks/LLDB.framework/Resources/"
        "debugserver",
    };
    for (const auto candidate : candidates)
    {
        const std::filesystem::path path{candidate};
        if (std::filesystem::exists(path))
        {
            setenv("LLDB_DEBUGSERVER_PATH", path.c_str(), 0);
            return;
        }
    }
#endif
}

auto map_state(lldb::StateType state) noexcept -> State
{
    if (state == lldb::eStateUnloaded) return State::unloaded;
    if (state == lldb::eStateConnected) return State::connected;
    if (state == lldb::eStateAttaching) return State::launching;
    if (state == lldb::eStateLaunching) return State::launching;
    if (state == lldb::eStateRunning) return State::running;
    if (state == lldb::eStateStepping) return State::stepping;
    if (state == lldb::eStateStopped) return State::stopped;
    if (state == lldb::eStateCrashed) return State::crashed;
    if (state == lldb::eStateDetached) return State::detached;
    if (state == lldb::eStateExited) return State::exited;
    if (state == lldb::eStateSuspended) return State::suspended;
    return State::invalid;
}

auto frame_file(lldb::SBFrame& frame) -> std::string
{
    auto line_entry = frame.GetLineEntry();
    auto file_spec = line_entry.GetFileSpec();
    const char* name = file_spec.GetFilename();
    if (name == nullptr)
    {
        return {};
    }
    const char* directory = file_spec.GetDirectory();
    if (directory == nullptr)
    {
        return std::string{name};
    }
    return std::string{directory} + "/" + name;
}

auto frame_function(lldb::SBFrame& frame) -> std::string
{
    const char* display = frame.GetDisplayFunctionName();
    if (display != nullptr)
    {
        return std::string{display};
    }
    const char* mangled = frame.GetFunctionName();
    return mangled != nullptr ? std::string{mangled} : std::string{};
}
}  // namespace

auto state_name(State state) noexcept -> std::string_view
{
    switch (state)
    {
        case State::invalid: return "invalid";
        case State::unloaded: return "unloaded";
        case State::connected: return "connected";
        case State::launching: return "launching";
        case State::running: return "running";
        case State::stepping: return "stepping";
        case State::stopped: return "stopped";
        case State::crashed: return "crashed";
        case State::detached: return "detached";
        case State::exited: return "exited";
        case State::suspended: return "suspended";
    }
    return "invalid";
}

auto stopped_for_inspection(State state) noexcept -> bool
{
    return state == State::stopped or state == State::crashed or state == State::suspended;
}

struct Debugger::Impl
{
    lldb::SBDebugger debugger{};
    lldb::SBTarget target{};
    lldb::SBProcess process{};
    std::string last_error{};

    auto build_stop_info() -> StopInfo
    {
        StopInfo info{};
        info.state = map_state(process.GetState());
        if (info.state == State::exited)
        {
            info.exit_status = process.GetExitStatus();
            return info;
        }
        if (not stopped_for_inspection(info.state))
        {
            return info;
        }

        auto thread = process.GetSelectedThread();
        info.thread_id = thread.GetThreadID();

        std::array<char, 256> description{};
        const auto written = thread.GetStopDescription(description.data(), description.size());
        if (written > 0)
        {
            info.description.assign(description.data());
        }

        auto frame = thread.GetSelectedFrame();
        info.function = frame_function(frame);
        info.file = frame_file(frame);
        info.line = frame.GetLineEntry().GetLine();
        return info;
    }
};

Debugger::Debugger() : impl_{std::make_unique<Impl>()}
{
    if (g_init_refcount == 0)
    {
        ensure_debugserver_env();
        lldb::SBDebugger::Initialize();
    }
    ++g_init_refcount;

    impl_->debugger = lldb::SBDebugger::Create();
    impl_->debugger.SetAsync(false);
}

Debugger::~Debugger()
{
    if (impl_)
    {
        if (impl_->process.IsValid())
        {
            impl_->process.Kill();
        }
        if (impl_->debugger.IsValid())
        {
            lldb::SBDebugger::Destroy(impl_->debugger);
        }
        if (--g_init_refcount == 0)
        {
            lldb::SBDebugger::Terminate();
        }
    }
}

Debugger::Debugger(Debugger&&) noexcept = default;
auto Debugger::operator=(Debugger&&) noexcept -> Debugger& = default;

auto Debugger::load_executable(std::string_view path) -> bool
{
    const std::string path_str{path};
    lldb::SBError error{};
    impl_->target = impl_->debugger.CreateTarget(path_str.c_str(), nullptr, nullptr, true, error);
    if (not impl_->target.IsValid())
    {
        impl_->last_error = error.IsValid() and error.GetCString() != nullptr
                                ? error.GetCString()
                                : "failed to create target";
        return false;
    }
    impl_->last_error.clear();
    return true;
}

auto Debugger::set_breakpoint(std::string_view file, u32 line) -> u32
{
    const std::string file_str{file};
    auto breakpoint = impl_->target.BreakpointCreateByLocation(file_str.c_str(), line);
    if (not breakpoint.IsValid() or breakpoint.GetNumLocations() == 0)
    {
        impl_->last_error = "no breakpoint location bound for " + file_str + ":"
                            + std::to_string(line);
        return 0;
    }
    const auto id = breakpoint.GetID();
    return id < 0 ? 0u : static_cast<u32>(id);
}

auto Debugger::set_breakpoint_by_name(std::string_view symbol) -> u32
{
    const std::string symbol_str{symbol};
    // Scope to the target executable module so a common name (e.g. "add") does
    // not also bind inside system libraries.
    const char* module = impl_->target.GetExecutable().GetFilename();
    auto breakpoint = module != nullptr
                          ? impl_->target.BreakpointCreateByName(symbol_str.c_str(), module)
                          : impl_->target.BreakpointCreateByName(symbol_str.c_str());
    if (not breakpoint.IsValid() or breakpoint.GetNumLocations() == 0)
    {
        impl_->last_error = "no breakpoint location bound for symbol " + symbol_str;
        return 0;
    }
    const auto id = breakpoint.GetID();
    return id < 0 ? 0u : static_cast<u32>(id);
}

auto Debugger::launch(std::span<const std::string> args) -> StopInfo
{
    lldb::SBLaunchInfo launch_info{nullptr};

    std::vector<const char*> argv{};
    if (not args.empty())
    {
        argv.reserve(args.size() + 1);
        for (const auto& arg : args)
        {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);
        launch_info.SetArguments(argv.data(), false);
    }

    lldb::SBError error{};
    impl_->process = impl_->target.Launch(launch_info, error);
    if (not impl_->process.IsValid())
    {
        impl_->last_error = error.IsValid() and error.GetCString() != nullptr
                                ? error.GetCString()
                                : "failed to launch process";
        StopInfo info{};
        info.state = State::invalid;
        return info;
    }
    impl_->last_error.clear();
    return impl_->build_stop_info();
}

auto Debugger::cont() -> StopInfo
{
    impl_->process.Continue();
    return impl_->build_stop_info();
}

auto Debugger::step_over() -> StopInfo
{
    impl_->process.GetSelectedThread().StepOver();
    auto info = impl_->build_stop_info();
    if (info.description.empty())
    {
        info.description = "step over";
    }
    return info;
}

auto Debugger::step_into() -> StopInfo
{
    impl_->process.GetSelectedThread().StepInto();
    auto info = impl_->build_stop_info();
    if (info.description.empty())
    {
        info.description = "step into";
    }
    return info;
}

auto Debugger::step_out() -> StopInfo
{
    impl_->process.GetSelectedThread().StepOut();
    auto info = impl_->build_stop_info();
    if (info.description.empty())
    {
        info.description = "step out";
    }
    return info;
}

auto Debugger::kill() -> void
{
    if (impl_->process.IsValid())
    {
        impl_->process.Kill();
    }
}

auto Debugger::state() const -> State
{
    if (not impl_->process.IsValid())
    {
        return impl_->target.IsValid() ? State::unloaded : State::invalid;
    }
    return map_state(impl_->process.GetState());
}

auto Debugger::backtrace() const -> std::vector<StackFrame>
{
    std::vector<StackFrame> frames{};
    if (not impl_->process.IsValid())
    {
        return frames;
    }
    auto thread = impl_->process.GetSelectedThread();
    const auto count = thread.GetNumFrames();
    frames.reserve(count);
    for (u32 i = 0; i < count; ++i)
    {
        auto frame = thread.GetFrameAtIndex(i);
        StackFrame out{};
        out.index = i;
        out.pc = frame.GetPC();
        out.function = frame_function(frame);
        out.file = frame_file(frame);
        out.line = frame.GetLineEntry().GetLine();
        frames.push_back(std::move(out));
    }
    return frames;
}

auto Debugger::locals() const -> std::vector<Variable>
{
    std::vector<Variable> variables{};
    if (not impl_->process.IsValid())
    {
        return variables;
    }
    auto frame = impl_->process.GetSelectedThread().GetSelectedFrame();
    auto values = frame.GetVariables(true, true, false, true);
    const auto count = values.GetSize();
    variables.reserve(count);
    for (u32 i = 0; i < count; ++i)
    {
        auto value = values.GetValueAtIndex(i);
        if (not value.IsValid())
        {
            continue;
        }
        Variable out{};
        const char* name = value.GetName();
        const char* type = value.GetTypeName();
        const char* repr = value.GetValue();
        if (repr == nullptr)
        {
            repr = value.GetSummary();
        }
        out.name = name != nullptr ? name : "";
        out.type = type != nullptr ? type : "";
        out.value = repr != nullptr ? repr : "";
        variables.push_back(std::move(out));
    }
    return variables;
}

auto Debugger::evaluate(std::string_view expr) const -> std::optional<std::string>
{
    if (not impl_->process.IsValid())
    {
        return std::nullopt;
    }
    const std::string expr_str{expr};
    auto frame = impl_->process.GetSelectedThread().GetSelectedFrame();
    auto value = frame.EvaluateExpression(expr_str.c_str());
    if (not value.IsValid())
    {
        return std::nullopt;
    }
    auto error = value.GetError();
    if (error.Fail())
    {
        return std::nullopt;
    }
    const char* repr = value.GetValue();
    if (repr == nullptr)
    {
        repr = value.GetSummary();
    }
    return repr != nullptr ? std::optional<std::string>{repr} : std::optional<std::string>{""};
}

auto Debugger::select_frame(u32 index) -> bool
{
    if (not impl_->process.IsValid())
    {
        return false;
    }
    auto thread = impl_->process.GetSelectedThread();
    if (index >= thread.GetNumFrames())
    {
        return false;
    }
    return thread.SetSelectedFrame(index).IsValid();
}

auto Debugger::drain_stdout() -> std::string
{
    std::string output{};
    if (not impl_->process.IsValid())
    {
        return output;
    }
    std::array<char, 1024> buffer{};
    for (;;)
    {
        const auto read = impl_->process.GetSTDOUT(buffer.data(), buffer.size());
        if (read == 0)
        {
            break;
        }
        output.append(buffer.data(), read);
    }
    return output;
}

auto Debugger::last_error() const -> std::string_view
{
    return impl_->last_error;
}
}  // namespace dans::dbg
