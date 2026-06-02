// include/dans/dev.hpp
// Externals
#include <dans/development_markers.hpp>
// StdLib
#include <string_view>
#include <utility>
//

#pragma once
#ifndef DANS_DEV_HPP
#define DANS_DEV_HPP

namespace dans::dev
{
[[noreturn]] def panic_impl(std::string_view msg, std::string_view file, int line) -> void;

template <typename F>
class Defer
{
  public:
    explicit Defer(F func) : func_{std::move(func)}
    {
    }
    ~Defer()
    {
        func_();
    }
    Defer(const Defer&) = delete;
    Defer(Defer&&) = delete;
    def operator=(const Defer&) -> Defer& = delete;
    def operator=(Defer&&) -> Defer& = delete;

  private:
    F func_;
};

template <typename F>
Defer(F) -> Defer<F>;

}  // namespace dans::dev

#define DANS_PANIC(msg) ::dans::dev::panic_impl((msg), __FILE__, __LINE__)

#endif  // DANS_DEV_HPP
