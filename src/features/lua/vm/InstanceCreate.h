#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace InstanceCreate {

bool New(const char* className, std::uint64_t parent, std::uint64_t* out_addr);
bool SetParent(std::uint64_t inst, std::uint64_t parent);
int LastFail();

} // namespace InstanceCreate
} // namespace Features
} // namespace Cheat
