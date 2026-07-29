#include "pch.h"
#include "LuaBridge.h"
#include "LuaTypes.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/offsets/Offsets.h"

#include <cstring>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaBridge {
namespace {

constexpr const char* k_mt = "jewsploit.Instance";

struct LuaInstance {
	std::uint64_t address{ 0 };
};

LuaInstance* CheckInst(lua_State* L, int idx = 1)
{
	return static_cast<LuaInstance*>(luaL_checkudata(L, idx, k_mt));
}

bool ValidAddr(std::uint64_t addr)
{
	return addr != 0 && g_Memory.IsValid(addr);
}

bool IsBasePartClass(const std::string& cls)
{
	return cls == "Part" || cls == "MeshPart" || cls == "BasePart" ||
		cls == "UnionOperation" || cls == "TrussPart" || cls == "WedgePart" ||
		cls == "CornerWedgePart" || cls == "SpawnLocation" || cls == "Seat" ||
		cls == "VehicleSeat";
}

int l_tostring(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	if (!ValidAddr(ud->address))
	{
		lua_pushstring(L, "Instance(nil)");
		return 1;
	}
	Instance inst(ud->address);
	const std::string name = inst.GetName();
	const std::string cls = inst.GetClassName();
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s (%s) @ 0x%llX",
		name.empty() ? "?" : name.c_str(),
		cls.empty() ? "?" : cls.c_str(),
		static_cast<unsigned long long>(ud->address));
	lua_pushstring(L, buf);
	return 1;
}

int l_eq(lua_State* L)
{
	LuaInstance* a = CheckInst(L, 1);
	LuaInstance* b = CheckInst(L, 2);
	lua_pushboolean(L, a->address == b->address);
	return 1;
}

int l_GetChildren(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	if (!ValidAddr(ud->address))
		return 1;

	const auto kids = Instance(ud->address).GetChildren();
	int i = 1;
	for (const auto& c : kids)
	{
		if (!ValidAddr(c.address))
			continue;
		PushInstance(L, c.address);
		lua_rawseti(L, -2, i++);
	}
	return 1;
}

int l_FindFirstChild(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}
	auto c = Instance(ud->address).FindFirstChild(name);
	if (!c || !ValidAddr(c->address))
		lua_pushnil(L);
	else
		PushInstance(L, c->address);
	return 1;
}

int l_FindFirstChildOfClass(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* cls = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !cls)
	{
		lua_pushnil(L);
		return 1;
	}
	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (Instance(c.address).GetClassName() == cls)
		{
			PushInstance(L, c.address);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

int l_IsA(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* cls = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !cls)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, Instance(ud->address).GetClassName() == cls);
	return 1;
}

int l_GetFullName(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	if (!ValidAddr(ud->address))
	{
		lua_pushstring(L, "");
		return 1;
	}

	std::vector<std::string> parts;
	Instance cur(ud->address);
	for (int depth = 0; depth < 64; ++depth)
	{
		if (!ValidAddr(cur.address))
			break;
		if (cur.GetClassName() == "DataModel")
			break;
		parts.push_back(cur.GetName());
		auto p = cur.GetParent();
		if (!p || !ValidAddr(p->address))
			break;
		cur = *p;
	}

	std::string full;
	for (int i = static_cast<int>(parts.size()) - 1; i >= 0; --i)
	{
		if (!full.empty())
			full += '.';
		full += parts[static_cast<size_t>(i)];
	}
	lua_pushstring(L, full.c_str());
	return 1;
}

int l_GetService(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* name = luaL_checkstring(L, 2);
	if (!ValidAddr(ud->address) || !name)
	{
		lua_pushnil(L);
		return 1;
	}

	for (const auto& c : Instance(ud->address).GetChildren())
	{
		Instance child(c.address);
		if (child.GetClassName() == name || child.GetName() == name)
		{
			PushInstance(L, c.address);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

int l_GetPlayers(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	if (!ValidAddr(ud->address))
		return 1;

	int i = 1;
	for (const auto& c : Instance(ud->address).GetChildren())
	{
		if (Instance(c.address).GetClassName() != "Player")
			continue;
		PushInstance(L, c.address);
		lua_rawseti(L, -2, i++);
	}
	return 1;
}

int l_index(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	const char* key = luaL_checkstring(L, 2);
	if (!key)
		return 0;

	luaL_getmetatable(L, k_mt);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);
	if (!lua_isnil(L, -1))
	{
		lua_remove(L, -2);
		return 1;
	}
	lua_pop(L, 2);

	if (!ValidAddr(ud->address))
	{
		lua_pushnil(L);
		return 1;
	}

	Instance inst(ud->address);
	const std::string cls = inst.GetClassName();

	if (std::strcmp(key, "Name") == 0)
	{
		lua_pushstring(L, inst.GetName().c_str());
		return 1;
	}
	if (std::strcmp(key, "ClassName") == 0)
	{
		lua_pushstring(L, cls.c_str());
		return 1;
	}
	if (std::strcmp(key, "Address") == 0)
	{
		lua_pushinteger(L, static_cast<lua_Integer>(ud->address));
		return 1;
	}
	if (std::strcmp(key, "Parent") == 0)
	{
		auto p = inst.GetParent();
		if (!p || !ValidAddr(p->address))
			lua_pushnil(L);
		else
			PushInstance(L, p->address);
		return 1;
	}

	if (cls == "DataModel")
	{
		DataModel dm(ud->address);
		if (std::strcmp(key, "PlaceId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(dm.GetPlaceId()));
			return 1;
		}
		if (std::strcmp(key, "GameId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(dm.GetGameId()));
			return 1;
		}
		if (std::strcmp(key, "JobId") == 0)
		{
			lua_pushstring(L, dm.GetJobId().c_str());
			return 1;
		}
		if (std::strcmp(key, "Workspace") == 0 || std::strcmp(key, "workspace") == 0)
		{
			if (Globals::Workspace && ValidAddr(Globals::Workspace->address))
				PushInstance(L, Globals::Workspace->address);
			else
				lua_pushnil(L);
			return 1;
		}
	}

	if (cls == "Players")
	{
		if (std::strcmp(key, "LocalPlayer") == 0)
		{
			const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
				ud->address + Offsets::Player::LocalPlayer);
			if (ValidAddr(lp))
				PushInstance(L, lp);
			else
				lua_pushnil(L);
			return 1;
		}
	}

	if (cls == "Player")
	{
		Player pl(ud->address);
		if (std::strcmp(key, "Character") == 0)
		{
			auto ch = pl.GetCharacter();
			if (!ch || !ValidAddr(ch->address))
				lua_pushnil(L);
			else
				PushInstance(L, ch->address);
			return 1;
		}
		if (std::strcmp(key, "DisplayName") == 0)
		{
			lua_pushstring(L, pl.GetDisplayName().c_str());
			return 1;
		}
		if (std::strcmp(key, "UserId") == 0)
		{
			lua_pushinteger(L, static_cast<lua_Integer>(pl.GetUserId()));
			return 1;
		}
	}

	if (cls == "Workspace")
	{
		if (std::strcmp(key, "CurrentCamera") == 0)
		{
			auto cam = Workspace(ud->address).GetCurrentCamera();
			if (!cam || !ValidAddr(cam->address))
				lua_pushnil(L);
			else
				PushInstance(L, cam->address);
			return 1;
		}
	}

	if (IsBasePartClass(cls))
	{
		BasePart part(ud->address);
		if (std::strcmp(key, "Position") == 0)
		{
			LuaTypes::PushVector3(L, part.GetPosition());
			return 1;
		}
		if (std::strcmp(key, "Size") == 0)
		{
			LuaTypes::PushVector3(L, part.GetSize());
			return 1;
		}
		if (std::strcmp(key, "CFrame") == 0)
		{
			LuaTypes::PushCFrame(L, part.GetPosition(), part.GetRotation());
			return 1;
		}
		if (std::strcmp(key, "Transparency") == 0)
		{
			lua_pushnumber(L, part.GetTransparency());
			return 1;
		}
		if (std::strcmp(key, "Anchored") == 0)
		{
			lua_pushboolean(L, part.IsAnchored());
			return 1;
		}
	}

	if (cls == "Humanoid")
	{
		Humanoid hum(ud->address);
		if (std::strcmp(key, "Health") == 0)
		{
			lua_pushnumber(L, hum.GetHealth());
			return 1;
		}
		if (std::strcmp(key, "MaxHealth") == 0)
		{
			lua_pushnumber(L, hum.GetMaxHealth());
			return 1;
		}
		if (std::strcmp(key, "WalkSpeed") == 0)
		{
			lua_pushnumber(L, hum.GetWalkSpeed());
			return 1;
		}
		if (std::strcmp(key, "DisplayName") == 0)
		{
			lua_pushstring(L, hum.GetDisplayName().c_str());
			return 1;
		}
		if (std::strcmp(key, "RootPart") == 0)
		{
			auto rp = hum.GetRootPart();
			if (!rp || !ValidAddr(rp->address))
				lua_pushnil(L);
			else
				PushInstance(L, rp->address);
			return 1;
		}
	}

	auto child = inst.FindFirstChild(key);
	if (child && ValidAddr(child->address))
	{
		PushInstance(L, child->address);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

constexpr size_t k_desc_cap = 50000;

void CollectDescendants(std::uint64_t addr, lua_State* L, int table_idx, int& n, size_t& nodes)
{
	if (!ValidAddr(addr) || nodes >= k_desc_cap)
		return;
	Instance inst(addr);
	for (const auto& c : inst.GetChildren())
	{
		if (nodes >= k_desc_cap)
			break;
		++nodes;
		++n;
		PushInstance(L, c.address);
		lua_rawseti(L, table_idx, n);
		CollectDescendants(c.address, L, table_idx, n, nodes);
	}
}

int l_GetDescendants(lua_State* L)
{
	LuaInstance* ud = CheckInst(L);
	lua_newtable(L);
	int n = 0;
	size_t nodes = 0;
	if (ValidAddr(ud->address))
		CollectDescendants(ud->address, L, lua_gettop(L), n, nodes);
	return 1;
}

void CreateMetatable(lua_State* L)
{
	if (luaL_newmetatable(L, k_mt))
	{
		lua_pushcfunction(L, l_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, l_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, l_eq);
		lua_setfield(L, -2, "__eq");

		lua_pushcfunction(L, l_GetChildren);
		lua_setfield(L, -2, "GetChildren");
		lua_pushcfunction(L, l_FindFirstChild);
		lua_setfield(L, -2, "FindFirstChild");
		lua_pushcfunction(L, l_FindFirstChildOfClass);
		lua_setfield(L, -2, "FindFirstChildOfClass");
		lua_pushcfunction(L, l_IsA);
		lua_setfield(L, -2, "IsA");
		lua_pushcfunction(L, l_GetFullName);
		lua_setfield(L, -2, "GetFullName");
		lua_pushcfunction(L, l_GetService);
		lua_setfield(L, -2, "GetService");
		lua_pushcfunction(L, l_GetPlayers);
		lua_setfield(L, -2, "GetPlayers");
		lua_pushcfunction(L, l_GetDescendants);
		lua_setfield(L, -2, "GetDescendants");
	}
	lua_pop(L, 1);
}

} // namespace

void PushInstance(lua_State* L, std::uint64_t address)
{
	auto* ud = static_cast<LuaInstance*>(lua_newuserdatauv(L, sizeof(LuaInstance), 0));
	ud->address = address;
	luaL_getmetatable(L, k_mt);
	lua_setmetatable(L, -2);
}

std::uint64_t CheckAddress(lua_State* L, int idx)
{
	auto* ud = static_cast<LuaInstance*>(luaL_checkudata(L, idx, "jewsploit.Instance"));
	return ud->address;
}

void RefreshGlobals(lua_State* L)
{
	if (ValidAddr(Globals::InstanceDataModel.address))
		PushInstance(L, Globals::InstanceDataModel.address);
	else
		lua_pushnil(L);
	lua_setglobal(L, "game");

	if (Globals::Workspace && ValidAddr(Globals::Workspace->address))
		PushInstance(L, Globals::Workspace->address);
	else
		lua_pushnil(L);
	lua_setglobal(L, "workspace");
}

void Register(lua_State* L)
{
	CreateMetatable(L);
	RefreshGlobals(L);
}

} // namespace LuaBridge
} // namespace Features
} // namespace Cheat
