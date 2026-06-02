// include/dans/chrono.hpp
// Externals
#include <dans/dev.hpp>
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <vector>
//

#pragma once
#ifndef DANS_CHRONO_HPP
#define DANS_CHRONO_HPP

namespace dans::chrono
{
[[nodiscard]] def format_seconds(std::chrono::duration<f64> dur, bool show_milliseconds = false)
    -> std::string;

class ScopeTimer
{
    using Clock = std::chrono::steady_clock;

  public:
    explicit ScopeTimer(std::string_view name, bool print_millis = false)
        : name_{name}, start_{Clock::now()}, print_millis_{print_millis}
    {
    }
    ~ScopeTimer()
    {
        try
        {
            const std::chrono::duration<f64> dur{Clock::now() - start_};
            std::println("{} finished in {}.", name_, format_seconds(dur, print_millis_));
        }
        catch (...)
        {
        }
    }
    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&) = delete;
    def operator=(const ScopeTimer&) -> ScopeTimer& = delete;
    def operator=(ScopeTimer&&) -> ScopeTimer& = delete;

  private:
    std::string_view name_{};
    std::chrono::time_point<Clock> start_{};
    bool print_millis_{false};
};

class StopWatch
{
    using Clock = std::chrono::steady_clock;
    static constexpr usize initial_capacity{4};

  public:
    StopWatch()
    {
        durations_.reserve(initial_capacity);
    }

    def start() -> void
    {
        if (running_) DANS_PANIC("StopWatch::start() called while already running");
        start_time_ = Clock::now();
        running_ = true;
    }

    def stop() -> void
    {
        if (not running_) DANS_PANIC("StopWatch::stop() called without prior start()");
        const std::chrono::duration<f64> elapsed{Clock::now() - start_time_};
        durations_.push_back(elapsed.count());
        running_ = false;
    }

    def abort() -> void
    {
        running_ = false;
    }

    def clear() -> void
    {
        durations_.clear();
        running_ = false;
    }

    [[nodiscard]] def durations() const -> const std::vector<f64>&
    {
        return durations_;
    }

    [[nodiscard]] def is_running() const -> bool
    {
        return running_;
    }

  private:
    std::vector<f64> durations_{};
    std::chrono::time_point<Clock> start_time_{};
    bool running_{false};
};

}  // namespace dans::chrono

#endif  // DANS_CHRONO_HPP
