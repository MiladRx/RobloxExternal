#pragma once

#include "../State.h"
#include "gui/colors/colors.h"
#include "imgui.h"

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline const char* LevelTag(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Print:   return "print";
	case LogLevel::Warn:    return "warn";
	case LogLevel::Error:   return "error";
	case LogLevel::Success: return "ok";
	default:                return "info";
	}
}

inline ImU32 LevelColor(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Print:
		return colors::text_active_u32();
	case LogLevel::Warn:
		return IM_COL32(230, 180, 70, 255);
	case LogLevel::Error:
		return IM_COL32(235, 85, 85, 255);
	case LogLevel::Success:
		return colors::accent_u32();
	default:
		return colors::text_inactive_u32();
	}
}

inline void PushOutput(LogLevel level, const char* text)
{
	if (!text)
		return;

	OutputLine line;
	line.level = level;
	line.text = text;
	g_output.push_back(std::move(line));

	// лог не бесконечный
	if (g_output.size() > 500)
		g_output.erase(g_output.begin(), g_output.begin() + (g_output.size() - 500));

	g_scroll_out = true;
}

}
}
}
