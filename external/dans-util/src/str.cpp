// src/str.cpp
//
#include <dans/str.hpp>
// Externals
#include <dans/development_markers.hpp>
// StdLib
#include <string_view>
#include <vector>
//

namespace dans::str
{
[[nodiscard]] def split(std::string_view text, char delim) -> std::vector<std::string_view>
{
    mut std::vector<std::string_view> out{};
    mut auto left = 0zu;
    while (true)
    {
        const auto pos = text.find(delim, left);
        if (pos == std::string_view::npos) break;
        out.push_back(text.substr(left, pos - left));
        left = pos + 1;
    }
    out.push_back(text.substr(left));
    return out;
}
}  // namespace dans::str
