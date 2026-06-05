#pragma once

#include "dans/dans-core/types.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dans::dbg
{
enum class State : u8
{
    invalid = 0,
    unloaded,
    connected,
    launching,
    running,
    stepping,
    stopped,
    crashed,
    detached,
    exited,
    suspended,
};

[[nodiscard]] auto state_name(State state) noexcept -> std::string_view;

[[nodiscard]] auto stopped_for_inspection(State state) noexcept -> bool;

struct StackFrame
{
    u32 index{};
    u64 pc{};
    std::string function{};
    std::string file{};
    u32 line{};
};

struct Variable
{
    std::string name{};
    std::string type{};
    std::string value{};
};

// Snapshot of where the inferior is after a launch/continue/step settles.
struct StopInfo
{
    State state{State::invalid};
    u64 thread_id{};
    std::string description{};  // e.g. "breakpoint 1.1", "step over"
    std::string function{};
    std::string file{};
    u32 line{};
    i32 exit_status{};  // valid only when state == exited
};

// Thin, frontend-facing wrapper over the LLDB SB API. No LLDB types leak through
// this header, so a Vulkan/ImGui frontend can drive a debug session without
// taking a compile dependency on liblldb. Drive it from a single thread; the
// blocking calls (launch/continue/step) return once the inferior settles.
class Debugger
{
  public:
    Debugger();
    ~Debugger();

    Debugger(const Debugger&) = delete;
    auto operator=(const Debugger&) -> Debugger& = delete;
    Debugger(Debugger&&) noexcept;
    auto operator=(Debugger&&) noexcept -> Debugger&;

    // Create a target from a locally built executable. Returns false on failure;
    // last_error() then carries the reason.
    [[nodiscard]] auto load_executable(std::string_view path) -> bool;

    // Returns the breakpoint id (>= 1) or 0 on failure.
    auto set_breakpoint(std::string_view file, u32 line) -> u32;
    auto set_breakpoint_by_name(std::string_view symbol) -> u32;

    // Launch / resume. Each blocks until the inferior next stops or exits.
    [[nodiscard]] auto launch(std::span<const std::string> args = {}) -> StopInfo;
    [[nodiscard]] auto cont() -> StopInfo;
    [[nodiscard]] auto step_over() -> StopInfo;
    [[nodiscard]] auto step_into() -> StopInfo;
    [[nodiscard]] auto step_out() -> StopInfo;

    auto kill() -> void;

    [[nodiscard]] auto state() const -> State;
    [[nodiscard]] auto backtrace() const -> std::vector<StackFrame>;
    [[nodiscard]] auto locals() const -> std::vector<Variable>;  // current frame
    [[nodiscard]] auto evaluate(std::string_view expr) const -> std::optional<std::string>;

    // Selects the active frame for backtrace()/locals()/evaluate() queries.
    auto select_frame(u32 index) -> bool;

    // Drains whatever the inferior has written to stdout since the last call.
    [[nodiscard]] auto drain_stdout() -> std::string;

    [[nodiscard]] auto last_error() const -> std::string_view;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace dans::dbg
