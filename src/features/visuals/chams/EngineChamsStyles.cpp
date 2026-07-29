#include "pch.h"
#include "EngineChamsStyles.h"
#include "EngineChamsApply.h"

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

// charm: Param = idx+1, ColorData white
std::uint32_t ColorParam(int idx)
{
	if (idx < 0)
	{
		idx = 0;
	}

	if (idx > 6)
	{
		idx = 6;
	}

	return (std::uint32_t)(idx + 1);
}

// charm material_set:
//   ghost     fill=0 flags2=0  param=color
//   wireframe fill=1 flags2=0  param=white (цвет не юзает)
//   mesh      fill=0 flags2=15 param=color  — без цвета = как default
//   charwire  fill=1 flags2=7  param=color
bool ApplyStyleLayers(uintptr_t ent, int style, int color_idx)
{
	const std::uint32_t color_param = ColorParam(color_idx);

	if (style == 1)
	{
		ApplyLayers(ent, 0, color_param, 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 2)
	{
		ApplyLayers(ent, 1, ColorParam(6), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 3)
	{
		ApplyLayers(ent, 0, color_param, 15u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 4)
	{
		ApplyLayers(ent, 1, color_param, 7u, 0xFFFFFFFFu);
		return true;
	}

	return false;
}

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
