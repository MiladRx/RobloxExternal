#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace Reflect {

// the engine interns every string (class name, method name) into RBX::Name
// and from then on carries only the pointer everywhere. the tables are read externally
std::uint64_t Name(std::uintptr_t base, const char* text);
std::uint64_t Creator(std::uintptr_t base, std::uint64_t name);

} // namespace Reflect
} // namespace Features
} // namespace Cheat
