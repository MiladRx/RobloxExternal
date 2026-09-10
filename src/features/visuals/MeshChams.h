#pragma once

#include "core/roblox/math/Math.h"
#include "imgui.h"

#include <cstdint>

namespace Cheat {
namespace Visuals {
namespace MeshChams {

// color: ImGui tris; shader: queue in MeshDxShader (Flush before ImGui GPU)
void Draw(
	ImDrawList* dl,
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	ImU32 fill_col);

// outline: each type = fade style + animation
const char* const* OutlineStyleNames();
int OutlineStyleNameCount();

// bounding type=mesh: screen/world AABB by real meshes (accessories too)
bool ExpandBounds(
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	float& min_x, float& max_x,
	float& min_y, float& max_y,
	Vector3& wmin, Vector3& wmax);

} // namespace MeshChams
} // namespace Visuals
} // namespace Cheat
