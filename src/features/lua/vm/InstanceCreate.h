#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace InstanceCreate {

bool New(const char* className, std::uint64_t parent, std::uint64_t* out_addr);
bool SetParent(std::uint64_t inst, std::uint64_t parent);

// writes a std::string at the field address
bool SetString(std::uint64_t field, const char* text);

// same, but for Content: the string address inside it, not the start of the object
bool SetContent(std::uint64_t string_field, const char* text);

int LastFail();

} // namespace InstanceCreate
} // namespace Features
} // namespace Cheat
