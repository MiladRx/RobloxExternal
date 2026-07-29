#include "pch.h"
#include "LuaDrawing.h"
#include "LuaTypes.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Cheat {
namespace Features {
namespace LuaDrawing {
namespace {

constexpr const char* k_mt = "jewsploit.Drawing";

enum class DrawType : int {
	Line = 0,
	Text,
	Circle,
	Square,
};

struct DrawObject {
	DrawType type{ DrawType::Line };
	bool alive{ true };
	bool visible{ true };
	float thickness{ 1.f };
	float transparency{ 0.f }; // 0 = opaque
	float r{ 1.f }, g{ 1.f }, b{ 1.f };
	float fromX{ 0 }, fromY{ 0 };
	float toX{ 0 }, toY{ 0 };
	float posX{ 0 }, posY{ 0 };
	float size{ 13.f };
	float radius{ 50.f };
	float width{ 100.f }, height{ 100.f };
	bool filled{ false };
	bool centered{ false };
	bool outline{ false };
	std::string text;
};

std::mutex g_mutex;
std::vector<std::shared_ptr<DrawObject>> g_objects;

struct LuaDraw {
	std::shared_ptr<DrawObject> obj;
};

LuaDraw* Check(lua_State* L, int idx = 1)
{
	return static_cast<LuaDraw*>(luaL_checkudata(L, idx, k_mt));
}

ImU32 MakeColor(const DrawObject& o)
{
	const float a = std::clamp(1.f - o.transparency, 0.f, 1.f);
	return IM_COL32(
		static_cast<int>(std::clamp(o.r, 0.f, 1.f) * 255.f),
		static_cast<int>(std::clamp(o.g, 0.f, 1.f) * 255.f),
		static_cast<int>(std::clamp(o.b, 0.f, 1.f) * 255.f),
		static_cast<int>(a * 255.f));
}

int l_remove(lua_State* L)
{
	auto* ud = Check(L);
	if (ud->obj)
	{
		ud->obj->alive = false;
		ud->obj->visible = false;
	}
	return 0;
}

int l_tostring(lua_State* L)
{
	auto* ud = Check(L);
	if (!ud->obj || !ud->obj->alive)
	{
		lua_pushstring(L, "Drawing(removed)");
		return 1;
	}
	const char* name = "Drawing";
	switch (ud->obj->type)
	{
	case DrawType::Line: name = "Drawing(Line)"; break;
	case DrawType::Text: name = "Drawing(Text)"; break;
	case DrawType::Circle: name = "Drawing(Circle)"; break;
	case DrawType::Square: name = "Drawing(Square)"; break;
	}
	lua_pushstring(L, name);
	return 1;
}

int l_index(lua_State* L)
{
	auto* ud = Check(L);
	const char* key = luaL_checkstring(L, 2);
	if (std::strcmp(key, "Remove") == 0 || std::strcmp(key, "Destroy") == 0)
	{
		lua_pushcfunction(L, l_remove);
		return 1;
	}
	if (!ud->obj || !ud->obj->alive)
	{
		lua_pushnil(L);
		return 1;
	}
	auto& o = *ud->obj;

	if (std::strcmp(key, "Visible") == 0) { lua_pushboolean(L, o.visible); return 1; }
	if (std::strcmp(key, "Transparency") == 0) { lua_pushnumber(L, o.transparency); return 1; }
	if (std::strcmp(key, "Thickness") == 0) { lua_pushnumber(L, o.thickness); return 1; }
	if (std::strcmp(key, "Color") == 0) { LuaTypes::PushColor3(L, o.r, o.g, o.b); return 1; }
	if (std::strcmp(key, "ZIndex") == 0) { lua_pushinteger(L, 0); return 1; }

	if (o.type == DrawType::Line)
	{
		if (std::strcmp(key, "From") == 0) { LuaTypes::PushVector2(L, o.fromX, o.fromY); return 1; }
		if (std::strcmp(key, "To") == 0) { LuaTypes::PushVector2(L, o.toX, o.toY); return 1; }
	}
	if (o.type == DrawType::Text)
	{
		if (std::strcmp(key, "Text") == 0) { lua_pushstring(L, o.text.c_str()); return 1; }
		if (std::strcmp(key, "Position") == 0) { LuaTypes::PushVector2(L, o.posX, o.posY); return 1; }
		if (std::strcmp(key, "Size") == 0) { lua_pushnumber(L, o.size); return 1; }
		if (std::strcmp(key, "Center") == 0) { lua_pushboolean(L, o.centered); return 1; }
		if (std::strcmp(key, "Outline") == 0) { lua_pushboolean(L, o.outline); return 1; }
	}
	if (o.type == DrawType::Circle)
	{
		if (std::strcmp(key, "Position") == 0) { LuaTypes::PushVector2(L, o.posX, o.posY); return 1; }
		if (std::strcmp(key, "Radius") == 0) { lua_pushnumber(L, o.radius); return 1; }
		if (std::strcmp(key, "Filled") == 0) { lua_pushboolean(L, o.filled); return 1; }
		if (std::strcmp(key, "NumSides") == 0) { lua_pushinteger(L, 64); return 1; }
	}
	if (o.type == DrawType::Square)
	{
		if (std::strcmp(key, "Position") == 0) { LuaTypes::PushVector2(L, o.posX, o.posY); return 1; }
		if (std::strcmp(key, "Size") == 0) { LuaTypes::PushVector2(L, o.width, o.height); return 1; }
		if (std::strcmp(key, "Filled") == 0) { lua_pushboolean(L, o.filled); return 1; }
	}

	lua_pushnil(L);
	return 1;
}

int l_newindex(lua_State* L)
{
	auto* ud = Check(L);
	if (!ud->obj || !ud->obj->alive)
		return 0;
	auto& o = *ud->obj;
	const char* key = luaL_checkstring(L, 2);

	if (std::strcmp(key, "Visible") == 0) { o.visible = lua_toboolean(L, 3) != 0; return 0; }
	if (std::strcmp(key, "Transparency") == 0) { o.transparency = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
	if (std::strcmp(key, "Thickness") == 0) { o.thickness = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
	if (std::strcmp(key, "Color") == 0)
	{
		float r, g, b;
		if (LuaTypes::ToColor3(L, 3, r, g, b))
		{
			o.r = r; o.g = g; o.b = b;
		}
		return 0;
	}

	if (o.type == DrawType::Line)
	{
		float x, y;
		if (std::strcmp(key, "From") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.fromX = x; o.fromY = y;
			return 0;
		}
		if (std::strcmp(key, "To") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.toX = x; o.toY = y;
			return 0;
		}
	}
	if (o.type == DrawType::Text)
	{
		if (std::strcmp(key, "Text") == 0)
		{
			o.text = luaL_checkstring(L, 3);
			return 0;
		}
		float x, y;
		if (std::strcmp(key, "Position") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.posX = x; o.posY = y;
			return 0;
		}
		if (std::strcmp(key, "Size") == 0) { o.size = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
		if (std::strcmp(key, "Center") == 0) { o.centered = lua_toboolean(L, 3) != 0; return 0; }
		if (std::strcmp(key, "Outline") == 0) { o.outline = lua_toboolean(L, 3) != 0; return 0; }
	}
	if (o.type == DrawType::Circle)
	{
		float x, y;
		if (std::strcmp(key, "Position") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.posX = x; o.posY = y;
			return 0;
		}
		if (std::strcmp(key, "Radius") == 0) { o.radius = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
		if (std::strcmp(key, "Filled") == 0) { o.filled = lua_toboolean(L, 3) != 0; return 0; }
	}
	if (o.type == DrawType::Square)
	{
		float x, y;
		if (std::strcmp(key, "Position") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.posX = x; o.posY = y;
			return 0;
		}
		if (std::strcmp(key, "Size") == 0 && LuaTypes::ToVector2(L, 3, x, y))
		{
			o.width = x; o.height = y;
			return 0;
		}
		if (std::strcmp(key, "Filled") == 0) { o.filled = lua_toboolean(L, 3) != 0; return 0; }
	}
	return 0;
}

int l_new(lua_State* L)
{
	const char* typeName = luaL_checkstring(L, 1);
	DrawType type = DrawType::Line;
	if (_stricmp(typeName, "Line") == 0) type = DrawType::Line;
	else if (_stricmp(typeName, "Text") == 0) type = DrawType::Text;
	else if (_stricmp(typeName, "Circle") == 0) type = DrawType::Circle;
	else if (_stricmp(typeName, "Square") == 0) type = DrawType::Square;
	else
		return luaL_error(L, "Drawing.new: unknown type '%s' (Line/Text/Circle/Square)", typeName);

	auto obj = std::make_shared<DrawObject>();
	obj->type = type;
	{
		std::lock_guard lock(g_mutex);
		g_objects.push_back(obj);
	}

	auto* ud = static_cast<LuaDraw*>(lua_newuserdatauv(L, sizeof(LuaDraw), 0));
	new (ud) LuaDraw{ obj };
	luaL_getmetatable(L, k_mt);
	lua_setmetatable(L, -2);
	return 1;
}

int l_clear(lua_State* L)
{
	(void)L;
	Clear();
	return 0;
}

int l_gc(lua_State* L)
{
	auto* ud = Check(L);
	ud->~LuaDraw();
	return 0;
}

} // namespace

void Clear()
{
	std::lock_guard lock(g_mutex);
	for (auto& o : g_objects)
	{
		if (o)
		{
			o->alive = false;
			o->visible = false;
		}
	}
	g_objects.clear();
}

void Render()
{
	std::vector<std::shared_ptr<DrawObject>> snapshot;
	{
		std::lock_guard lock(g_mutex);
		g_objects.erase(
			std::remove_if(g_objects.begin(), g_objects.end(),
				[](const std::shared_ptr<DrawObject>& o) { return !o || !o->alive; }),
			g_objects.end());
		snapshot = g_objects;
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;

	for (const auto& sp : snapshot)
	{
		if (!sp || !sp->alive || !sp->visible)
			continue;
		const auto& o = *sp;
		const ImU32 col = MakeColor(o);
		const float th = std::max(1.f, o.thickness);

		switch (o.type)
		{
		case DrawType::Line:
			dl->AddLine(ImVec2(o.fromX, o.fromY), ImVec2(o.toX, o.toY), col, th);
			break;
		case DrawType::Text:
		{
			ImVec2 pos(o.posX, o.posY);
			if (o.centered)
			{
				const ImVec2 sz = ImGui::CalcTextSize(o.text.c_str());
				pos.x -= sz.x * 0.5f;
				pos.y -= sz.y * 0.5f;
			}
			if (o.outline)
			{
				const ImU32 oc = IM_COL32(0, 0, 0, (col >> 24) & 0xFF);
				dl->AddText(ImVec2(pos.x - 1, pos.y), oc, o.text.c_str());
				dl->AddText(ImVec2(pos.x + 1, pos.y), oc, o.text.c_str());
				dl->AddText(ImVec2(pos.x, pos.y - 1), oc, o.text.c_str());
				dl->AddText(ImVec2(pos.x, pos.y + 1), oc, o.text.c_str());
			}
			dl->AddText(nullptr, o.size > 0.f ? o.size : 0.f, pos, col, o.text.c_str());
			break;
		}
		case DrawType::Circle:
			if (o.filled)
				dl->AddCircleFilled(ImVec2(o.posX, o.posY), o.radius, col, 64);
			else
				dl->AddCircle(ImVec2(o.posX, o.posY), o.radius, col, 64, th);
			break;
		case DrawType::Square:
		{
			const ImVec2 a(o.posX, o.posY);
			const ImVec2 b(o.posX + o.width, o.posY + o.height);
			if (o.filled)
				dl->AddRectFilled(a, b, col);
			else
				dl->AddRect(a, b, col, 0.f, 0, th);
			break;
		}
		}
	}
}

void Register(lua_State* L)
{
	if (luaL_newmetatable(L, k_mt))
	{
		lua_pushcfunction(L, l_index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, l_newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, l_tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, l_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushcfunction(L, l_new);
	lua_setfield(L, -2, "new");
	lua_pushcfunction(L, l_clear);
	lua_setfield(L, -2, "clear");
	lua_setglobal(L, "Drawing");

	lua_pushcfunction(L, l_clear);
	lua_setglobal(L, "cleardrawcache");
}

} // namespace LuaDrawing
} // namespace Features
} // namespace Cheat
