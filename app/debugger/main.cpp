#include "dans/dbg/debugger.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
auto print_usage(const char* executable) -> void
{
    std::cout << "usage: " << executable
              << " <program> [--break file:line] [--break-fn symbol]"
                 " [--steps N] [-- prog-args...]\n";
}

auto print_stop(const dans::dbg::StopInfo& stop) -> void
{
    std::cout << "[" << dans::dbg::state_name(stop.state) << "]";
    if (not stop.description.empty())
    {
        std::cout << " " << stop.description;
    }
    if (not stop.function.empty())
    {
        std::cout << " in " << stop.function;
        if (not stop.file.empty())
        {
            std::cout << " at " << stop.file << ":" << stop.line;
        }
    }
    std::cout << '\n';
}

auto print_session(dans::dbg::Debugger& debugger) -> void
{
    const auto frames = debugger.backtrace();
    const auto shown = frames.size() < 6 ? frames.size() : static_cast<dans::usize>(6);
    for (dans::usize i = 0; i < shown; ++i)
    {
        const auto& frame = frames[i];
        std::cout << "  #" << frame.index << " " << frame.function;
        if (not frame.file.empty())
        {
            std::cout << "  (" << frame.file << ":" << frame.line << ")";
        }
        std::cout << '\n';
    }
    for (const auto& var : debugger.locals())
    {
        std::cout << "    " << var.type << " " << var.name << " = " << var.value << '\n';
    }
}
}  // namespace

auto main(int argc, char** argv) -> int
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    std::string program{};
    std::vector<std::pair<std::string, dans::u32>> line_breaks{};
    std::vector<std::string> symbol_breaks{};
    std::vector<std::string> inferior_args{};
    dans::u32 steps = 0;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};
        if (arg == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "--")
        {
            for (int j = i + 1; j < argc; ++j)
            {
                inferior_args.emplace_back(argv[j]);
            }
            break;
        }
        if (arg == "--break" and i + 1 < argc)
        {
            const std::string spec{argv[++i]};
            const auto colon = spec.rfind(':');
            if (colon == std::string::npos)
            {
                std::cerr << "expected file:line, got: " << spec << '\n';
                return 2;
            }
            line_breaks.emplace_back(spec.substr(0, colon),
                                     static_cast<dans::u32>(std::stoul(spec.substr(colon + 1))));
        }
        else if (arg == "--break-fn" and i + 1 < argc)
        {
            symbol_breaks.emplace_back(argv[++i]);
        }
        else if (arg == "--steps" and i + 1 < argc)
        {
            steps = static_cast<dans::u32>(std::stoul(argv[++i]));
        }
        else if (program.empty() and not arg.starts_with("--"))
        {
            program = arg;
        }
        else
        {
            std::cerr << "unknown or incomplete argument: " << arg << '\n';
            print_usage(argv[0]);
            return 2;
        }
    }

    if (program.empty())
    {
        std::cerr << "no program given\n";
        print_usage(argv[0]);
        return 2;
    }

    dans::dbg::Debugger debugger{};
    if (not debugger.load_executable(program))
    {
        std::cerr << "load failed: " << debugger.last_error() << '\n';
        return 1;
    }
    std::cout << "target: " << program << '\n';

    for (const auto& [file, line] : line_breaks)
    {
        const auto id = debugger.set_breakpoint(file, line);
        if (id == 0)
        {
            std::cerr << "breakpoint failed: " << debugger.last_error() << '\n';
            return 1;
        }
        std::cout << "breakpoint " << id << " at " << file << ":" << line << '\n';
    }
    for (const auto& symbol : symbol_breaks)
    {
        const auto id = debugger.set_breakpoint_by_name(symbol);
        if (id == 0)
        {
            std::cerr << "breakpoint failed: " << debugger.last_error() << '\n';
            return 1;
        }
        std::cout << "breakpoint " << id << " at " << symbol << '\n';
    }

    auto stop = debugger.launch(inferior_args);
    if (stop.state == dans::dbg::State::invalid)
    {
        std::cerr << "launch failed: " << debugger.last_error() << '\n';
        return 1;
    }

    auto flush_output = [&]() -> void
    {
        const auto out = debugger.drain_stdout();
        if (not out.empty())
        {
            std::cout << "--- inferior stdout ---\n" << out << "-----------------------\n";
        }
    };

    while (dans::dbg::stopped_for_inspection(stop.state))
    {
        print_stop(stop);
        print_session(debugger);
        flush_output();

        if (steps > 0)
        {
            stop = debugger.step_over();
            --steps;
        }
        else
        {
            stop = debugger.cont();
        }
    }

    print_stop(stop);
    flush_output();
    if (stop.state == dans::dbg::State::exited)
    {
        std::cout << "exit status: " << stop.exit_status << '\n';
        return stop.exit_status;
    }
    return 0;
}
