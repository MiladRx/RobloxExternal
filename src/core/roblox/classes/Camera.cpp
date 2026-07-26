#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

Vector3 Cheat::Camera::GetPosition() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(address + Offsets::Camera::Position);
}

Matrix4x4 Cheat::Camera::GetRotation() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	float rot[9];
	g_Memory.ReadRaw(address + Offsets::Camera::Rotation, &rot, sizeof(rot));

	return Matrix4x4(
		rot[0], rot[1], rot[2], 0.f,
		rot[3], rot[4], rot[5], 0.f,
		rot[6], rot[7], rot[8], 0.f,
		0.f, 0.f, 0.f, 1.f
	);
}

float Cheat::Camera::GetFieldOfView() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + Offsets::Camera::FieldOfView);
}

Vector2 Cheat::Camera::GetViewportSize() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Vector2>(address + Offsets::Camera::ViewportSize);
}

void Cheat::Camera::SetFieldOfView(float fov) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + Offsets::Camera::FieldOfView, fov);
}

void Cheat::Camera::SetRotation(const Matrix4x4& rot) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	// обратно в 3x3, движок так хранит
	float r[9] = {
		rot.m[0][0], rot.m[0][1], rot.m[0][2],
		rot.m[1][0], rot.m[1][1], rot.m[1][2],
		rot.m[2][0], rot.m[2][1], rot.m[2][2]
	};
	g_Memory.WriteRaw(address + Offsets::Camera::Rotation, &r, sizeof(r));
}

bool Cheat::Camera::WorldToScreen(const Vector3& worldPos, Vector2& screenPos) const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	Matrix4x4 view = GetRotation();
	Vector3 cam = GetPosition();
	Vector2 vp = GetViewportSize();

	Vector3 rel = worldPos - cam;

	float x = rel.x * view.m[0][0] + rel.y * view.m[0][1] + rel.z * view.m[0][2];
	float y = rel.x * view.m[1][0] + rel.y * view.m[1][1] + rel.z * view.m[1][2];
	float z = rel.x * view.m[2][0] + rel.y * view.m[2][1] + rel.z * view.m[2][2];

	// за камерой нахуй
	if (z > 0.f)
	{
		return false;
	}

	float fov = GetFieldOfView();
	float tan_half = tanf(fov * 0.5f * (3.1415926535f / 180.f));
	float aspect = vp.x / vp.y;

	float sx = (vp.x / 2.f) * (1.f + (x / (-z * tan_half * aspect)));
	float sy = (vp.y / 2.f) * (1.f - (y / (-z * tan_half)));

	screenPos = { sx, sy };
	return true;
}
