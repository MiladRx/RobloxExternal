#include "pch.h"
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "LuaExecutor.h"

#include "handlers/Draw.h"
#include "app/Settings.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"

#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace Cheat::Features {

//constexpr size_t k_editor_cap = 256 * 1024;
//constexpr size_t k_output_cap = 500;

void LuaExecutor::Initialize()
{
	if (LuaDetail::g_inited)
		return;

	LuaDetail::g_inited = true;
	LuaDetail::EnsureDefaultTab();
	LuaDetail::RefreshScripts();
}

void LuaExecutor::Shutdown()
{
	LuaDetail::g_tabs.clear();
	LuaDetail::g_scripts.clear();
	LuaDetail::g_output.clear();
	LuaDetail::g_tab = 0;
	LuaDetail::g_inited = false;
}

void LuaExecutor::ClearOutput()
{
	LuaDetail::g_output.clear();
}

void LuaExecutor::Log(LogLevel level, const char* fmt, ...)
{
	if (!fmt)
		return;

	char buf[2048];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	LuaDetail::PushOutput(level, buf);
}

void LuaExecutor::Render(float alpha)
{
	if (!g_Settings.lua.executor)
		return;
	if (alpha <= 0.001f)
		return;

	Initialize();

	float now = (float)ImGui::GetTime();
	if (now >= LuaDetail::g_refresh_at)
	{
		LuaDetail::RefreshScripts();
		LuaDetail::g_refresh_at = now + 1.5f;
	}

	ImGui::SetNextWindowSize(ImVec2(760.f, 500.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 340.f), ImVec2(1600.f, 1100.f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
	LuaDetail::PushChrome();

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

	if (ImGui::Begin("##jewsploit_lua_executor", nullptr, flags))
	{
		colors::draw_panel_background(alpha);

		float margin = 10.f;
		ImVec2 win = ImGui::GetWindowPos();
		ImVec2 sz = ImGui::GetWindowSize();
		ImVec2 panel_min(win.x + margin, win.y + margin);
		float panel_w = sz.x - margin * 2.f;
		float panel_h = sz.y - margin * 2.f;

		ImGui::SetCursorPos(ImVec2(margin, margin));
		if (widgets::begin_child_panel(
				"lua_root",
				ImVec2(panel_w, panel_h),
				"lua", fonts::ui_bold(), LuaDetail::PanelFs(),
				nullptr, nullptr, nullptr))
		{
			LuaDetail::DrawTabStrip(ImGui::GetContentRegionAvail().x - 8.f);

			float side = 160.f;
			float gap = 4.f;
			float btn_row = 20.f + 8.f;
			float body_w = ImGui::GetContentRegionAvail().x - 8.f;
			float stack_h = ImGui::GetContentRegionAvail().y - btn_row - 4.f;
			if (stack_h < 140.f)
				stack_h = 140.f;

			float editor_w = body_w - side - gap;
			if (editor_w < 120.f)
				editor_w = 120.f;

			float output_h = stack_h * 0.32f;
			if (output_h < 90.f)
				output_h = 90.f;
			if (output_h > 180.f)
				output_h = 180.f;

			float editor_h = stack_h - output_h - gap;
			if (editor_h < 80.f)
				editor_h = 80.f;

			float body_y = ImGui::GetCursorPosY();

			ImGui::SetCursorPos(ImVec2(6.f, body_y));
			LuaDetail::DrawEditor(editor_w, editor_h);
			ImGui::SetCursorPos(ImVec2(6.f, body_y + editor_h + gap));
			LuaDetail::DrawOutput(editor_w, output_h);

			ImGui::SetCursorPos(ImVec2(6.f + editor_w + gap, body_y));
			LuaDetail::DrawScriptList(side, stack_h);

			float btn_y = body_y + stack_h + 4.f;
			float btn_gap = 4.f;
			float left_btn_w = (editor_w - btn_gap * 3.f) / 4.f;

			ImGui::SetCursorPos(ImVec2(6.f, btn_y));
			if (widgets::button("run", ImVec2(left_btn_w, 0.f)))
			{
				(void)LuaDetail::CurEditor().GetText();
				Log(LogLevel::Info, "running...");
				Log(LogLevel::Warn, "lua vm stub пока нет");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("copy", ImVec2(left_btn_w, 0.f)))
			{
				LuaDetail::CopyCurrent();
				Log(LogLevel::Info, "copied to clipboard");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("save", ImVec2(left_btn_w, 0.f)))
			{
				if (LuaDetail::SaveCurrent())
					Log(LogLevel::Success, "saved %s", LuaDetail::Cur().name.c_str());

				else
					Log(LogLevel::Error, "failed to save script");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("clear", ImVec2(left_btn_w, 0.f)))
			{
				LuaDetail::ClearCurrent();
				Log(LogLevel::Info, "editor cleared");
			}

			ImGui::SetCursorPos(ImVec2(6.f + editor_w + gap, btn_y));
			if (widgets::button("open folder", ImVec2(side, 0.f)))
			{
				LuaDetail::OpenScriptsFolder();
			}
		}
		widgets::end_child_panel();

		if (LuaDetail::DrawHeaderClose(panel_min, panel_w))
			g_Settings.lua.executor = false;
	}
	ImGui::End();
	LuaDetail::PopChrome();
	ImGui::PopStyleVar();
}

}
