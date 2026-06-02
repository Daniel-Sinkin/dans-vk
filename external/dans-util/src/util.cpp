// src/util.cpp
//
#include <dans/util.hpp>
//
// External
#include <dans/types.hpp>

namespace dans::util
{
static_assert(sizeof(usize) >= sizeof(void*));
}  // namespace dans::util
