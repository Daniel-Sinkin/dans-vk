// include/dans/overload.hpp

#pragma once
#ifndef DANS_OVERLOAD_HPP
#define DANS_OVERLOAD_HPP

namespace dans
{
template <typename... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};

template <typename... Ts>
overload(Ts...) -> overload<Ts...>;
}  // namespace dans

#endif  // DANS_OVERLOAD_HPP
