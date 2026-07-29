#include "pch.h"
#include "LuaTypes.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "core/roblox/math/Math.h"

#include <cstring>
#include <cstdio>

namespace Cheat {
namespace Features {
namespace LuaTypes {
namespace {

constexpr const char* k_v3 = "jewsploit.Vector3";
constexpr const char* k_v2 = "jewsploit.Vector2";
constexpr const char* k_c3 = "jewsploit.Color3";
constexpr const char* k_cf = "jewsploit.CFrame";

struct LuaV3 { float x, y, z; };
struct LuaV2 { float x, y; };
struct LuaC3 { float r, g, b; };
struct LuaCF {
	float px, py, pz;
	float r00, r01, r02;
	float r10, r11, r12;
	float r20, r21, r22;
};

LuaV3* CheckV3(lua_State* L, int idx) { return static_cast<LuaV3*>(luaL_checkudata(L, idx, k_v3)); }
LuaV2* CheckV2(lua_State* L, int idx) { return static_cast<LuaV2*>(luaL_checkudata(L, idx, k_v2)); }
LuaC3* CheckC3(lua_State* L, int idx) { return static_cast<LuaC3*>(luaL_checkudata(L, idx, k_c3)); }
LuaCF* CheckCF(lua_State* L, int idx) { return static_cast<LuaCF*>(luaL_checkudata(L, idx, k_cf)); }

void PushV3Raw(lua_State* L, float x, float y, float z)
{
	auto* ud = static_cast<LuaV3*>(lua_newuserdatauv(L, sizeof(LuaV3), 0));
	ud->x = x; ud->y = y; ud->z = z;
	luaL_getmetatable(L, k_v3);
	lua_setmetatable(L, -2);
}

void PushV2Raw(lua_State* L, float x, float y)
{
	auto* ud = static_cast<LuaV2*>(lua_newuserdatauv(L, sizeof(LuaV2), 0));
	ud->x = x; ud->y = y;
	luaL_getmetatable(L, k_v2);
	lua_setmetatable(L, -2);
}

void PushC3Raw(lua_State* L, float r, float g, float b)
{
	auto* ud = static_cast<LuaC3*>(lua_newuserdatauv(L, sizeof(LuaC3), 0));
	ud->r = r; ud->g = g; ud->b = b;
	luaL_getmetatable(L, k_c3);
	lua_setmetatable(L, -2);
}

void PushCFRaw(lua_State* L, const LuaCF& cf)
{
	auto* ud = static_cast<LuaCF*>(lua_newuserdatauv(L, sizeof(LuaCF), 0));
	*ud = cf;
	luaL_getmetatable(L, k_cf);
	lua_setmetatable(L, -2);
}

// ---------- Vector3 ----------
int v3_dot(lua_State* L);
int v3_cross(lua_State* L);
int v3_lerp(lua_State* L);

int v3_new(lua_State* L)
{
	float x = 0, y = 0, z = 0;
	if (lua_isuserdata(L, 1) && luaL_testudata(L, 1, k_v3))
	{
		auto* v = CheckV3(L, 1);
		x = v->x; y = v->y; z = v->z;
	}
	else
	{
		x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
		y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
		z = static_cast<float>(luaL_optnumber(L, 3, 0.0));
	}
	PushV3Raw(L, x, y, z);
	return 1;
}

int v3_index(lua_State* L)
{
	auto* v = CheckV3(L, 1);
	const char* key = luaL_checkstring(L, 2);
	if (std::strcmp(key, "X") == 0 || std::strcmp(key, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
	if (std::strcmp(key, "Y") == 0 || std::strcmp(key, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
	if (std::strcmp(key, "Z") == 0 || std::strcmp(key, "z") == 0) { lua_pushnumber(L, v->z); return 1; }
	if (std::strcmp(key, "Magnitude") == 0)
	{
		lua_pushnumber(L, std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z));
		return 1;
	}
	if (std::strcmp(key, "Unit") == 0)
	{
		const float len = std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
		if (len > 1e-8f)
			PushV3Raw(L, v->x / len, v->y / len, v->z / len);
		else
			PushV3Raw(L, 0, 0, 0);
		return 1;
	}
	if (std::strcmp(key, "Dot") == 0) { lua_pushcfunction(L, v3_dot); return 1; }
	if (std::strcmp(key, "Cross") == 0) { lua_pushcfunction(L, v3_cross); return 1; }
	if (std::strcmp(key, "Lerp") == 0) { lua_pushcfunction(L, v3_lerp); return 1; }
	return 0;
}

int v3_tostring(lua_State* L)
{
	auto* v = CheckV3(L, 1);
	char buf[96];
	std::snprintf(buf, sizeof(buf), "%g, %g, %g", v->x, v->y, v->z);
	lua_pushstring(L, buf);
	return 1;
}

int v3_add(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	PushV3Raw(L, a->x + b->x, a->y + b->y, a->z + b->z);
	return 1;
}

int v3_sub(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	PushV3Raw(L, a->x - b->x, a->y - b->y, a->z - b->z);
	return 1;
}

int v3_mul(lua_State* L)
{
	if (lua_isnumber(L, 1))
	{
		const float s = static_cast<float>(lua_tonumber(L, 1));
		auto* v = CheckV3(L, 2);
		PushV3Raw(L, v->x * s, v->y * s, v->z * s);
	}
	else
	{
		auto* v = CheckV3(L, 1);
		const float s = static_cast<float>(luaL_checknumber(L, 2));
		PushV3Raw(L, v->x * s, v->y * s, v->z * s);
	}
	return 1;
}

int v3_div(lua_State* L)
{
	auto* v = CheckV3(L, 1);
	const float s = static_cast<float>(luaL_checknumber(L, 2));
	PushV3Raw(L, v->x / s, v->y / s, v->z / s);
	return 1;
}

int v3_unm(lua_State* L)
{
	auto* v = CheckV3(L, 1);
	PushV3Raw(L, -v->x, -v->y, -v->z);
	return 1;
}

int v3_eq(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	lua_pushboolean(L, a->x == b->x && a->y == b->y && a->z == b->z);
	return 1;
}

int v3_dot(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	lua_pushnumber(L, a->x * b->x + a->y * b->y + a->z * b->z);
	return 1;
}

int v3_cross(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	PushV3Raw(L,
		a->y * b->z - a->z * b->y,
		a->z * b->x - a->x * b->z,
		a->x * b->y - a->y * b->x);
	return 1;
}

int v3_lerp(lua_State* L)
{
	auto* a = CheckV3(L, 1);
	auto* b = CheckV3(L, 2);
	const float t = static_cast<float>(luaL_checknumber(L, 3));
	PushV3Raw(L,
		a->x + (b->x - a->x) * t,
		a->y + (b->y - a->y) * t,
		a->z + (b->z - a->z) * t);
	return 1;
}

// ---------- Vector2 ----------
int v2_new(lua_State* L)
{
	const float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
	const float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
	PushV2Raw(L, x, y);
	return 1;
}

int v2_index(lua_State* L)
{
	auto* v = CheckV2(L, 1);
	const char* key = luaL_checkstring(L, 2);
	if (std::strcmp(key, "X") == 0 || std::strcmp(key, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
	if (std::strcmp(key, "Y") == 0 || std::strcmp(key, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
	if (std::strcmp(key, "Magnitude") == 0)
	{
		lua_pushnumber(L, std::sqrt(v->x * v->x + v->y * v->y));
		return 1;
	}
	return 0;
}

int v2_tostring(lua_State* L)
{
	auto* v = CheckV2(L, 1);
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%g, %g", v->x, v->y);
	lua_pushstring(L, buf);
	return 1;
}

int v2_add(lua_State* L)
{
	auto* a = CheckV2(L, 1);
	auto* b = CheckV2(L, 2);
	PushV2Raw(L, a->x + b->x, a->y + b->y);
	return 1;
}

int v2_sub(lua_State* L)
{
	auto* a = CheckV2(L, 1);
	auto* b = CheckV2(L, 2);
	PushV2Raw(L, a->x - b->x, a->y - b->y);
	return 1;
}

int v2_mul(lua_State* L)
{
	if (lua_isnumber(L, 1))
	{
		const float s = static_cast<float>(lua_tonumber(L, 1));
		auto* v = CheckV2(L, 2);
		PushV2Raw(L, v->x * s, v->y * s);
	}
	else
	{
		auto* v = CheckV2(L, 1);
		const float s = static_cast<float>(luaL_checknumber(L, 2));
		PushV2Raw(L, v->x * s, v->y * s);
	}
	return 1;
}

int v2_div(lua_State* L)
{
	auto* v = CheckV2(L, 1);
	const float s = static_cast<float>(luaL_checknumber(L, 2));
	PushV2Raw(L, v->x / s, v->y / s);
	return 1;
}

// ---------- Color3 ----------
int c3_new(lua_State* L)
{
	const float r = static_cast<float>(luaL_optnumber(L, 1, 0.0));
	const float g = static_cast<float>(luaL_optnumber(L, 2, 0.0));
	const float b = static_cast<float>(luaL_optnumber(L, 3, 0.0));
	PushC3Raw(L, r, g, b);
	return 1;
}

int c3_fromRGB(lua_State* L)
{
	const float r = static_cast<float>(luaL_optnumber(L, 1, 0.0)) / 255.f;
	const float g = static_cast<float>(luaL_optnumber(L, 2, 0.0)) / 255.f;
	const float b = static_cast<float>(luaL_optnumber(L, 3, 0.0)) / 255.f;
	PushC3Raw(L, r, g, b);
	return 1;
}

int c3_toRGB(lua_State* L);

int c3_index(lua_State* L)
{
	auto* c = CheckC3(L, 1);
	const char* key = luaL_checkstring(L, 2);
	if (std::strcmp(key, "R") == 0 || std::strcmp(key, "r") == 0) { lua_pushnumber(L, c->r); return 1; }
	if (std::strcmp(key, "G") == 0 || std::strcmp(key, "g") == 0) { lua_pushnumber(L, c->g); return 1; }
	if (std::strcmp(key, "B") == 0 || std::strcmp(key, "b") == 0) { lua_pushnumber(L, c->b); return 1; }
	if (std::strcmp(key, "ToRGB") == 0) { lua_pushcfunction(L, c3_toRGB); return 1; }
	return 0;
}

int c3_tostring(lua_State* L)
{
	auto* c = CheckC3(L, 1);
	char buf[96];
	std::snprintf(buf, sizeof(buf), "%g, %g, %g", c->r, c->g, c->b);
	lua_pushstring(L, buf);
	return 1;
}

int c3_toRGB(lua_State* L)
{
	auto* c = CheckC3(L, 1);
	lua_pushinteger(L, static_cast<lua_Integer>(std::lround(c->r * 255.f)));
	lua_pushinteger(L, static_cast<lua_Integer>(std::lround(c->g * 255.f)));
	lua_pushinteger(L, static_cast<lua_Integer>(std::lround(c->b * 255.f)));
	return 3;
}

// ---------- CFrame ----------
LuaCF IdentityCF(float x, float y, float z)
{
	LuaCF cf{};
	cf.px = x; cf.py = y; cf.pz = z;
	cf.r00 = 1; cf.r11 = 1; cf.r22 = 1;
	return cf;
}

int cf_new(lua_State* L)
{
	LuaCF cf = IdentityCF(0, 0, 0);
	const int n = lua_gettop(L);
	if (n >= 1 && luaL_testudata(L, 1, k_v3))
	{
		auto* v = CheckV3(L, 1);
		cf.px = v->x; cf.py = v->y; cf.pz = v->z;
	}
	else if (n >= 3)
	{
		cf.px = static_cast<float>(lua_tonumber(L, 1));
		cf.py = static_cast<float>(lua_tonumber(L, 2));
		cf.pz = static_cast<float>(lua_tonumber(L, 3));
	}
	else if (n >= 1)
	{
		cf.px = static_cast<float>(luaL_optnumber(L, 1, 0.0));
		cf.py = static_cast<float>(luaL_optnumber(L, 2, 0.0));
		cf.pz = static_cast<float>(luaL_optnumber(L, 3, 0.0));
	}
	PushCFRaw(L, cf);
	return 1;
}

int cf_index(lua_State* L)
{
	auto* cf = CheckCF(L, 1);
	const char* key = luaL_checkstring(L, 2);
	if (std::strcmp(key, "Position") == 0 || std::strcmp(key, "p") == 0)
	{
		PushV3Raw(L, cf->px, cf->py, cf->pz);
		return 1;
	}
	if (std::strcmp(key, "X") == 0) { lua_pushnumber(L, cf->px); return 1; }
	if (std::strcmp(key, "Y") == 0) { lua_pushnumber(L, cf->py); return 1; }
	if (std::strcmp(key, "Z") == 0) { lua_pushnumber(L, cf->pz); return 1; }
	if (std::strcmp(key, "LookVector") == 0)
	{
		// Roblox: -Z column of rotation
		PushV3Raw(L, -cf->r02, -cf->r12, -cf->r22);
		return 1;
	}
	if (std::strcmp(key, "RightVector") == 0)
	{
		PushV3Raw(L, cf->r00, cf->r10, cf->r20);
		return 1;
	}
	if (std::strcmp(key, "UpVector") == 0)
	{
		PushV3Raw(L, cf->r01, cf->r11, cf->r21);
		return 1;
	}
	return 0;
}

int cf_tostring(lua_State* L)
{
	auto* cf = CheckCF(L, 1);
	char buf[128];
	std::snprintf(buf, sizeof(buf), "%g, %g, %g", cf->px, cf->py, cf->pz);
	lua_pushstring(L, buf);
	return 1;
}

int cf_mul(lua_State* L)
{
	auto* cf = CheckCF(L, 1);
	if (luaL_testudata(L, 2, k_v3))
	{
		auto* v = CheckV3(L, 2);
		const float x = cf->r00 * v->x + cf->r01 * v->y + cf->r02 * v->z + cf->px;
		const float y = cf->r10 * v->x + cf->r11 * v->y + cf->r12 * v->z + cf->py;
		const float z = cf->r20 * v->x + cf->r21 * v->y + cf->r22 * v->z + cf->pz;
		PushV3Raw(L, x, y, z);
		return 1;
	}
	luaL_error(L, "CFrame * Vector3 expected");
	return 0;
}

void RegisterVector3(lua_State* L)
{
	if (luaL_newmetatable(L, k_v3))
	{
		lua_pushcfunction(L, v3_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, v3_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, v3_add);
		lua_setfield(L, -2, "__add");
		lua_pushcfunction(L, v3_sub);
		lua_setfield(L, -2, "__sub");
		lua_pushcfunction(L, v3_mul);
		lua_setfield(L, -2, "__mul");
		lua_pushcfunction(L, v3_div);
		lua_setfield(L, -2, "__div");
		lua_pushcfunction(L, v3_unm);
		lua_setfield(L, -2, "__unm");
		lua_pushcfunction(L, v3_eq);
		lua_setfield(L, -2, "__eq");
		lua_pushcfunction(L, v3_dot);
		lua_setfield(L, -2, "Dot");
		lua_pushcfunction(L, v3_cross);
		lua_setfield(L, -2, "Cross");
		lua_pushcfunction(L, v3_lerp);
		lua_setfield(L, -2, "Lerp");
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, v3_new);
	lua_setfield(L, -2, "new");
	lua_pushcfunction(L, v3_new);
	lua_setfield(L, -2, "__call");
	lua_newtable(L);
	lua_pushcfunction(L, v3_new);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	PushV3Raw(L, 0, 0, 0); lua_setfield(L, -2, "zero");
	PushV3Raw(L, 1, 1, 1); lua_setfield(L, -2, "one");
	PushV3Raw(L, 0, 1, 0); lua_setfield(L, -2, "yAxis");
	PushV3Raw(L, 1, 0, 0); lua_setfield(L, -2, "xAxis");
	PushV3Raw(L, 0, 0, 1); lua_setfield(L, -2, "zAxis");
	lua_setglobal(L, "Vector3");
}

void RegisterVector2(lua_State* L)
{
	if (luaL_newmetatable(L, k_v2))
	{
		lua_pushcfunction(L, v2_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, v2_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, v2_add);
		lua_setfield(L, -2, "__add");
		lua_pushcfunction(L, v2_sub);
		lua_setfield(L, -2, "__sub");
		lua_pushcfunction(L, v2_mul);
		lua_setfield(L, -2, "__mul");
		lua_pushcfunction(L, v2_div);
		lua_setfield(L, -2, "__div");
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, v2_new);
	lua_setfield(L, -2, "new");
	lua_newtable(L);
	lua_pushcfunction(L, v2_new);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	lua_setglobal(L, "Vector2");
}

void RegisterColor3(lua_State* L)
{
	if (luaL_newmetatable(L, k_c3))
	{
		lua_pushcfunction(L, c3_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, c3_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, c3_toRGB);
		lua_setfield(L, -2, "ToRGB");
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, c3_new);
	lua_setfield(L, -2, "new");
	lua_pushcfunction(L, c3_fromRGB);
	lua_setfield(L, -2, "fromRGB");
	lua_newtable(L);
	lua_pushcfunction(L, c3_new);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	lua_setglobal(L, "Color3");
}

void RegisterCFrame(lua_State* L)
{
	if (luaL_newmetatable(L, k_cf))
	{
		lua_pushcfunction(L, cf_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, cf_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, cf_mul);
		lua_setfield(L, -2, "__mul");
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, cf_new);
	lua_setfield(L, -2, "new");
	lua_newtable(L);
	lua_pushcfunction(L, cf_new);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	lua_setglobal(L, "CFrame");
}

} // namespace

void PushVector3(lua_State* L, const Vector3& v)
{
	PushV3Raw(L, v.x, v.y, v.z);
}

bool ToVector3(lua_State* L, int idx, Vector3& out)
{
	if (luaL_testudata(L, idx, k_v3))
	{
		auto* v = CheckV3(L, idx);
		out = Vector3(v->x, v->y, v->z);
		return true;
	}
	if (lua_istable(L, idx))
	{
		lua_getfield(L, idx, "x");
		if (!lua_isnumber(L, -1)) lua_getfield(L, idx, "X");
		out.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_getfield(L, idx, "y");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "Y"); }
		out.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_getfield(L, idx, "z");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "Z"); }
		out.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		return true;
	}
	return false;
}

void PushVector2(lua_State* L, float x, float y)
{
	PushV2Raw(L, x, y);
}

bool ToVector2(lua_State* L, int idx, float& x, float& y)
{
	if (luaL_testudata(L, idx, k_v2))
	{
		auto* v = CheckV2(L, idx);
		x = v->x; y = v->y;
		return true;
	}
	if (lua_istable(L, idx))
	{
		lua_getfield(L, idx, "x");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "X"); }
		x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_getfield(L, idx, "y");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "Y"); }
		y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		return true;
	}
	return false;
}

void PushColor3(lua_State* L, float r, float g, float b)
{
	PushC3Raw(L, r, g, b);
}

bool ToColor3(lua_State* L, int idx, float& r, float& g, float& b)
{
	if (luaL_testudata(L, idx, k_c3))
	{
		auto* c = CheckC3(L, idx);
		r = c->r; g = c->g; b = c->b;
		return true;
	}
	if (lua_istable(L, idx))
	{
		lua_getfield(L, idx, "R");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "r"); }
		r = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_getfield(L, idx, "G");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "g"); }
		g = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_getfield(L, idx, "B");
		if (!lua_isnumber(L, -1)) { lua_pop(L, 1); lua_getfield(L, idx, "b"); }
		b = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		return true;
	}
	return false;
}

void PushCFrame(lua_State* L, const Vector3& pos, const Matrix4x4& rot)
{
	LuaCF cf{};
	cf.px = pos.x; cf.py = pos.y; cf.pz = pos.z;
	cf.r00 = rot.m[0][0]; cf.r01 = rot.m[0][1]; cf.r02 = rot.m[0][2];
	cf.r10 = rot.m[1][0]; cf.r11 = rot.m[1][1]; cf.r12 = rot.m[1][2];
	cf.r20 = rot.m[2][0]; cf.r21 = rot.m[2][1]; cf.r22 = rot.m[2][2];
	PushCFRaw(L, cf);
}

bool ToCFrame(lua_State* L, int idx, Vector3& pos, Matrix4x4& rot)
{
	if (!luaL_testudata(L, idx, k_cf))
		return false;
	auto* cf = CheckCF(L, idx);
	pos = Vector3(cf->px, cf->py, cf->pz);
	rot = Matrix4x4();
	rot.m[0][0] = cf->r00; rot.m[0][1] = cf->r01; rot.m[0][2] = cf->r02;
	rot.m[1][0] = cf->r10; rot.m[1][1] = cf->r11; rot.m[1][2] = cf->r12;
	rot.m[2][0] = cf->r20; rot.m[2][1] = cf->r21; rot.m[2][2] = cf->r22;
	return true;
}

void Register(lua_State* L)
{
	RegisterVector3(L);
	RegisterVector2(L);
	RegisterColor3(L);
	RegisterCFrame(L);
}

} // namespace LuaTypes
} // namespace Features
} // namespace Cheat
