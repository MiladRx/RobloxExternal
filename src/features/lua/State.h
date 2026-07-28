#pragma once

#include "LuaExecutor.h"
#include "gui/TextEditor/TextEditor.h"

#include <memory>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaDetail {

using LogLevel = LuaExecutor::LogLevel;

struct EditorTab {
	std::string name;
	std::string path;
	std::unique_ptr<TextEditor> editor;
};

struct OutputLine {
	LogLevel level = LogLevel::Info;
	std::string text;
};

inline std::vector<EditorTab> g_tabs;
inline int g_tab = 0;
inline std::vector<std::string> g_scripts;
inline std::vector<OutputLine> g_output;
inline bool g_scroll_out = false;
inline float g_refresh_at = 0.f;
inline bool g_inited = false;

}
}
}
