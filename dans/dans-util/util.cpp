// dans/dans-util/util.cpp
//
#include "dans/dans-util/util.hpp"
//
// External
#include "dans/dans-core/types.hpp"

namespace dans::util
{
static_assert(sizeof(usize) >= sizeof(void*));
}  // namespace dans::util
