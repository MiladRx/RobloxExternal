#pragma once

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaGc {

void Register(lua_State* L);

} // namespace LuaGc
} // namespace Features
} // namespace Cheat
