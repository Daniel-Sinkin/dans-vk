// dans/dans-util/random.cpp
//
#include "dans/dans-util/random.hpp"
// Externals
#include "dans/dans-core/development_markers.hpp"
#include "dans/dans-core/types.hpp"
// StdLib
#include <random>
//

namespace dans::random
{
[[nodiscard]] def rng() -> std::mt19937_64&
{
    mut_unchecked thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

def seed(u64 value) -> void
{
    rng().seed(value);
}
}  // namespace dans::random
