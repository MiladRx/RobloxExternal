#pragma once

#include <string>

struct lua_State;

namespace Cheat {
namespace Features {
namespace LuaVM {

bool Initialize();
void Shutdown();

// true = ok; ошибка уже в output executor
bool Execute(const std::string& source, const char* chunk_name = "script");

lua_State* State();
bool Ready();

// resume yields (task.wait) — звать из тика/рендера
void Tick(float dt);

} // namespace LuaVM
} // namespace Features
} // namespace Cheat
