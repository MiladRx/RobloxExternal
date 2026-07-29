#include "pch.h"
#include "LuaVM.h"
#include "LuaBridge.h"
#include "LuaTypes.h"
#include "LuaDrawing.h"
#include "LuaScripts.h"
#include "features/lua/LuaExecutor.h"
#include "renderer/Renderer.h"

#include <Windows.h>
#include <cctype>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaVM {
namespace {

lua_State* g_L = nullptr;

struct WaitEntry {
	int ref = LUA_NOREF;
	float left = 0.f;
};
std::vector<WaitEntry> g_waits;

void LogInfo(const char* msg)
{
	LuaExecutor::Log(LuaExecutor::LogLevel::Print, "%s", msg ? msg : "");
}

void LogErr(const char* msg)
{
	LuaExecutor::Log(LuaExecutor::LogLevel::Error, "%s", msg ? msg : "");
}

int l_print(lua_State* L)
{
	const int n = lua_gettop(L);
	std::string line;
	lua_getglobal(L, "tostring");
	for (int i = 1; i <= n; ++i)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		if (lua_pcall(L, 1, 1, 0) != LUA_OK)
		{
			LogErr(lua_tostring(L, -1));
			lua_pop(L, 1);
			continue;
		}
		const char* s = lua_tostring(L, -1);
		if (i > 1)
			line.push_back('\t');
		line += s ? s : "nil";
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	LogInfo(line.c_str());
	return 0;
}

int l_warn(lua_State* L)
{
	const int n = lua_gettop(L);
	std::string line;
	for (int i = 1; i <= n; ++i)
	{
		const char* s = luaL_tolstring(L, i, nullptr);
		if (i > 1)
			line.push_back('\t');
		line += s ? s : "nil";
		lua_pop(L, 1);
	}
	LuaExecutor::Log(LuaExecutor::LogLevel::Warn, "%s", line.c_str());
	return 0;
}

int l_identifyexecutor(lua_State* L)
{
	lua_pushstring(L, "jewsploit");
	lua_pushstring(L, "0.1");
	return 2;
}

int l_wait(lua_State* L)
{
	float t = static_cast<float>(luaL_optnumber(L, 1, 0.03));
	if (t < 0.f)
		t = 0.f;

	lua_pushthread(L);
	const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	g_waits.push_back({ ref, t });
	return lua_yield(L, 0);
}

int l_spawn(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_State* co = lua_newthread(L);
	lua_pushvalue(L, 1);
	lua_xmove(L, co, 1);
	// Lua 5.4: nresults must be a valid int* (writes through it)
	int nres = 0;
	const int status = lua_resume(co, L, 0, &nres);
	if (status != LUA_OK && status != LUA_YIELD)
	{
		LogErr(lua_tostring(co, -1));
		lua_pop(co, 1);
	}
	// on YIELD wait() already registry-refs the thread; stack ref can go
	lua_pop(L, 1); // thread
	return 0;
}

int l_getmousepos(lua_State* L)
{
	POINT pt{};
	GetCursorPos(&pt);
	if (HWND hwnd = Renderer::GetHwnd())
		ScreenToClient(hwnd, &pt);
	LuaTypes::PushVector2(L, static_cast<float>(pt.x), static_cast<float>(pt.y));
	return 1;
}

int l_isrbxactive(lua_State* L)
{
	lua_pushboolean(L, Renderer::IsGameActive() ? 1 : 0);
	return 1;
}

int l_iskeypressed(lua_State* L)
{
	int vk = 0;
	if (lua_type(L, 1) == LUA_TSTRING)
	{
		const char* s = lua_tostring(L, 1);
		if (!s || !s[0]) { lua_pushboolean(L, 0); return 1; }
		if (_stricmp(s, "left") == 0 || _stricmp(s, "lmb") == 0) vk = VK_LBUTTON;
		else if (_stricmp(s, "right") == 0 || _stricmp(s, "rmb") == 0) vk = VK_RBUTTON;
		else if (_stricmp(s, "middle") == 0 || _stricmp(s, "mmb") == 0) vk = VK_MBUTTON;
		else if (_stricmp(s, "space") == 0) vk = VK_SPACE;
		else if (_stricmp(s, "shift") == 0) vk = VK_SHIFT;
		else if (_stricmp(s, "ctrl") == 0) vk = VK_CONTROL;
		else if (_stricmp(s, "alt") == 0) vk = VK_MENU;
		else if (_stricmp(s, "tab") == 0) vk = VK_TAB;
		else if (_stricmp(s, "escape") == 0 || _stricmp(s, "esc") == 0) vk = VK_ESCAPE;
		else vk = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(s[0])));
	}
	else
		vk = static_cast<int>(luaL_checkinteger(L, 1));
	lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) != 0);
	return 1;
}

void OpenSafeLibs(lua_State* L)
{
	luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
	lua_pop(L, 1);
	luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
	lua_pop(L, 1);

	// убрать опасное из base
	lua_pushnil(L);
	lua_setglobal(L, "dofile");
	lua_pushnil(L);
	lua_setglobal(L, "loadfile");
}

void RegisterApi(lua_State* L)
{
	lua_pushcfunction(L, l_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, l_warn);
	lua_setglobal(L, "warn");
	lua_pushcfunction(L, l_identifyexecutor);
	lua_setglobal(L, "identifyexecutor");
	lua_pushcfunction(L, l_wait);
	lua_setglobal(L, "wait");
	lua_pushcfunction(L, l_spawn);
	lua_setglobal(L, "spawn");
	lua_pushcfunction(L, l_getmousepos);
	lua_setglobal(L, "getmousepos");
	lua_pushcfunction(L, l_isrbxactive);
	lua_setglobal(L, "isrbxactive");
	lua_pushcfunction(L, l_iskeypressed);
	lua_setglobal(L, "iskeypressed");

	lua_newtable(L);
	lua_pushcfunction(L, l_wait);
	lua_setfield(L, -2, "wait");
	lua_pushcfunction(L, l_spawn);
	lua_setfield(L, -2, "spawn");
	lua_setglobal(L, "task");

	LuaTypes::Register(L);
	LuaDrawing::Register(L);
	LuaScripts::Register(L);
	LuaBridge::Register(L);
}

} // namespace

bool Initialize()
{
	if (g_L)
		return true;

	g_L = luaL_newstate();
	if (!g_L)
		return false;

	OpenSafeLibs(g_L);
	RegisterApi(g_L);
	return true;
}

void Shutdown()
{
	LuaDrawing::Clear();

	if (!g_L)
		return;

	for (auto& w : g_waits)
	{
		if (w.ref != LUA_NOREF)
			luaL_unref(g_L, LUA_REGISTRYINDEX, w.ref);
	}
	g_waits.clear();

	lua_close(g_L);
	g_L = nullptr;
}

lua_State* State()
{
	return g_L;
}

bool Ready()
{
	return g_L != nullptr;
}

bool Execute(const std::string& source, const char* chunk_name)
{
	if (!Initialize() || !g_L)
	{
		LogErr("lua vm init failed");
		return false;
	}

	LuaBridge::RefreshGlobals(g_L);

	const char* name = chunk_name ? chunk_name : "script";
	if (luaL_loadbuffer(g_L, source.data(), source.size(), name) != LUA_OK)
	{
		LogErr(lua_tostring(g_L, -1));
		lua_pop(g_L, 1);
		return false;
	}

	lua_State* co = lua_newthread(g_L);
	lua_pushvalue(g_L, -2); // function
	lua_xmove(g_L, co, 1);
	lua_pop(g_L, 1); // remove original function from main

	int nres = 0;
	const int status = lua_resume(co, g_L, 0, &nres);
	if (status == LUA_OK)
	{
		lua_pop(g_L, 1); // thread
		LuaExecutor::Log(LuaExecutor::LogLevel::Success, "ok");
		return true;
	}
	if (status == LUA_YIELD)
	{
		// keep thread alive via wait registry — already handled if wait() was used
		// if yielded without wait, just drop
		lua_pop(g_L, 1);
		return true;
	}

	LogErr(lua_tostring(co, -1));
	lua_pop(co, 1);
	lua_pop(g_L, 1);
	return false;
}

void Tick(float dt)
{
	if (!g_L || g_waits.empty())
		return;

	// cap resumes/frame so wait(0) loops cannot freeze the UI thread
	constexpr int k_max_resumes = 64;
	int resumes = 0;

	for (size_t i = 0; i < g_waits.size();)
	{
		g_waits[i].left -= dt;
		if (g_waits[i].left > 0.f)
		{
			++i;
			continue;
		}

		if (resumes >= k_max_resumes)
		{
			g_waits[i].left = 0.f; // retry next frame
			++i;
			continue;
		}

		const int ref = g_waits[i].ref;
		lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref); // keep thread alive on stack
		lua_State* co = lua_tothread(g_L, -1);

		g_waits.erase(g_waits.begin() + static_cast<std::ptrdiff_t>(i));
		luaL_unref(g_L, LUA_REGISTRYINDEX, ref);
		++resumes;

		if (!co)
		{
			lua_pop(g_L, 1);
			continue;
		}

		int nres = 0;
		const int status = lua_resume(co, g_L, 0, &nres);
		if (status == LUA_OK)
		{
			LuaExecutor::Log(LuaExecutor::LogLevel::Success, "ok");
		}
		else if (status != LUA_YIELD)
		{
			LogErr(lua_tostring(co, -1));
			lua_pop(co, 1);
		}
		// YIELD: wait() already registry-refs again
		lua_pop(g_L, 1); // thread
		// do not ++i — new/remaining entries may sit at this index
	}
}

} // namespace LuaVM
} // namespace Features
} // namespace Cheat
