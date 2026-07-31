#include "pch.h"
#include "InstanceCreate.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"

namespace Cheat {
namespace Features {
namespace InstanceCreate {
namespace {

int g_last_fail = 0;

} // namespace

int LastFail()
{
	return g_last_fail;
}

// хуки (CRT/raycast/WHJob vptr) крашили клиент — пока выключено
bool New(const char* className, std::uint64_t parent, std::uint64_t* out_addr)
{
	(void)className;
	(void)parent;
	g_last_fail = -99;
	if (out_addr)
		*out_addr = 0;
	return false;
}

bool SetParent(std::uint64_t inst, std::uint64_t parent)
{
	if (!g_Memory.IsValid(inst))
	{
		g_last_fail = 1;
		return false;
	}

	// parent=0 = unparent ок
	if (parent != 0 && !g_Memory.IsValid(parent))
	{
		g_last_fail = 2;
		return false;
	}

	bool ok = Instance(inst).SetParent(parent);
	g_last_fail = ok ? 0 : 3;
	return ok;
}

} // namespace InstanceCreate
} // namespace Features
} // namespace Cheat
