#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace Reflect {

// движок интернирует каждую строку (имя класса, имя метода) в RBX::Name
// и дальше везде таскает только указатель. таблицы читаются снаружи
std::uint64_t Name(std::uintptr_t base, const char* text);
std::uint64_t Creator(std::uintptr_t base, std::uint64_t name);

} // namespace Reflect
} // namespace Features
} // namespace Cheat
