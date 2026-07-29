#pragma once

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

std::uint32_t ColorParam(int idx);
bool ApplyStyleLayers(uintptr_t ent, int style, int color_idx);

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
