#pragma once

#include "../execute/Scripts.h"
#include "../execute/Tabs.h"
#include "../protocol/Editor.h"
#include "../protocol/Log.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline bool DrawHeaderClose(const ImVec2& panel_min, float panel_w)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float sz = 12.f;
	float pad = 6.f;
	float header_h = 22.f;
	const ImVec2 mn(
		panel_min.x + panel_w - pad - sz - 2.0f,
		panel_min.y + (header_h - sz) * 0.5f + 1.0f);
	const ImVec2 mx(mn.x + sz, mn.y + sz);

	ImGui::SetCursorScreenPos(mn);
	const bool pressed = ImGui::InvisibleButton("##lua_close", ImVec2(sz, sz));
	const bool hovered = ImGui::IsItemHovered();
	const float t = hovered ? 1.0f : 0.0f;

	const ImU32 col = colors::label_u32(t);
	const float inset = 3.0f;
	dl->AddLine(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset), col, 1.25f);
	dl->AddLine(ImVec2(mx.x - inset, mn.y + inset), ImVec2(mn.x + inset, mx.y - inset), col, 1.25f);
	return pressed;
}

inline void DrawTabStrip(float avail_w)
{
	ImFont* font = fonts::ui();
	float fs = fonts::ui_size(font);
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGui::SetCursorPos(ImVec2(6.f, ImGui::GetCursorPosY() + 3.f));
	ImVec2 start = ImGui::GetCursorScreenPos();
	float x = start.x;
	float h = 17.f;
	float max_x = start.x + avail_w - 28.f;

	int close_idx = -1;

	for (int i = 0; i < (int)g_tabs.size(); ++i)
	{
		const char* label = "tab";
		if (!g_tabs[i].name.empty())
			label = g_tabs[i].name.c_str();

		ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
		float pad_x = 8.f;
		float close_w = 12.f;
		float w = ts.x + pad_x * 2.f + close_w;
		if (x + w > max_x && i > 0)
			break;

		ImVec2 mn(x, start.y);
		ImVec2 mx(x + w, start.y + h);
		bool active = (i == g_tab);

		ImGui::SetCursorScreenPos(mn);
		ImGui::PushID(i + 1000);
		ImGui::InvisibleButton("##tab", ImVec2(w - close_w, h));
		bool tab_pressed = ImGui::IsItemClicked();
		bool tab_hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		if (active)
		{
			dl->AddRectFilled(
				ImVec2(mn.x, mx.y - 1.f),
				ImVec2(mx.x - close_w, mx.y),
				colors::accent_u32());
		}

		else if (tab_hovered)
		{
			ImVec4 a = colors::accent;
			a.w = 0.12f;
			dl->AddRectFilled(mn, ImVec2(mx.x - close_w, mx.y), ImGui::ColorConvertFloat4ToU32(a));
		}

		float label_t = 0.f;
		if (tab_hovered)
			label_t = 1.f;
		ImU32 label_col = colors::label_u32(label_t);
		if (active)
			label_col = colors::accent_u32();

		widgets::draw_outlined_text(
			dl, font, fs,
			ImVec2(ImFloor(mn.x + pad_x), ImFloor(mn.y + (h - fs) * 0.5f)),
			label_col,
			label);

		ImVec2 cx0(mx.x - close_w, mn.y);
		ImGui::SetCursorScreenPos(cx0);
		ImGui::PushID(i + 2000);
		bool x_pressed = ImGui::InvisibleButton("##tx", ImVec2(close_w, h));
		bool x_hov = ImGui::IsItemHovered();
		ImGui::PopID();

		{
			float inset = 4.f;
			float xt = 0.35f;
			if (x_hov)
				xt = 1.f;
			ImU32 xc = colors::label_u32(xt);
			dl->AddLine(ImVec2(cx0.x + inset, mn.y + inset),
				ImVec2(cx0.x + close_w - inset, mx.y - inset), xc, 1.1f);
			dl->AddLine(ImVec2(cx0.x + close_w - inset, mn.y + inset),
				ImVec2(cx0.x + inset, mx.y - inset), xc, 1.1f);
		}

		if (tab_pressed)
			g_tab = i;
		if (x_pressed)
			close_idx = i;
		x += w + 1.f;
	}

	{
		float pw = 16.f;
		ImVec2 mn(x + 2.f, start.y);
		ImGui::SetCursorScreenPos(mn);
		bool pressed = ImGui::InvisibleButton("##tab_new", ImVec2(pw, h));
		bool hovered = ImGui::IsItemHovered();
		float ht = 0.f;
		if (hovered)
			ht = 1.f;
		widgets::draw_outlined_text(
			dl, font, fs,
			ImVec2(ImFloor(mn.x + 3.f), ImFloor(mn.y + (h - fs) * 0.5f)),
			colors::label_u32(ht),
			"+");
		if (pressed)
			NewBlankTab();
	}

	if (close_idx >= 0)
		CloseTab(close_idx);

	ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + h + 3.f));
	ImGui::Dummy(ImVec2(1.f, 0.f));
}

inline void DrawScriptList(float width, float height)
{
	ImFont* font = fonts::ui();
	float fs = fonts::ui_size(font);
	float tf = PanelFs();
	float row_h = 18.f;

	if (widgets::begin_child_panel(
			"lua_scripts", ImVec2(width, height),
			"scripts", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
		if (ImGui::BeginChild("##lua_script_scroll", ImVec2(0, 0), ImGuiChildFlags_None))
		{
			if (g_scripts.empty())
			{
				ImGui::SetCursorPos(ImVec2(6.f, 4.f));
				widgets::draw_outlined_text(
					ImGui::GetWindowDrawList(), font, fs,
					ImGui::GetCursorScreenPos(),
					colors::text_inactive_u32(), "no scripts");
				ImGui::Dummy(ImVec2(1, row_h));
			}

			for (int i = 0; i < (int)g_scripts.size(); ++i)
			{
				ImGui::PushID(i);
				ImVec2 row = ImGui::GetCursorScreenPos();
				bool selected = false;
				if (!g_tabs.empty() &&
					g_tabs[g_tab].path == (ScriptsDir() / g_scripts[i]).string())
				{
					selected = true;
				}

				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
					ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.20f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive,
					ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.32f));
				bool clicked = ImGui::Selectable("##s", selected, 0, ImVec2(0, row_h));
				ImGui::PopStyleColor(3);

				if (selected)
				{
					ImVec4 a = colors::accent;
					a.w = 0.14f;
					ImGui::GetWindowDrawList()->AddRectFilled(
						row, ImVec2(row.x + ImGui::GetContentRegionAvail().x + 6.f, row.y + row_h),
						ImGui::ColorConvertFloat4ToU32(a));
				}

				ImU32 name_col = colors::text_active_u32();
				if (selected)
					name_col = colors::accent_u32();

				widgets::draw_outlined_text(
					ImGui::GetWindowDrawList(), font, fs,
					ImVec2(ImFloor(row.x + 6.f), ImFloor(row.y + (row_h - fs) * 0.5f)),
					name_col,
					g_scripts[i].c_str());
				ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + row_h));

				if (clicked)
					LoadFileIntoTab(ScriptsDir() / g_scripts[i]);
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}
	widgets::end_child_panel();
}

inline void DrawOutput(float width, float height)
{
	float tf = PanelFs();
	if (!widgets::begin_child_panel(
			"lua_output", ImVec2(width, height),
			"output", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		widgets::end_child_panel();
		return;
	}

	ImFont* font = fonts::ui();
	if (fonts::proggy_clean)
		font = fonts::proggy_clean;
	float fs = fonts::ui_size(font);
	float row_h = 18.f;

	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float btn_w = 44.f;
		float bx = avail.x - btn_w - 4.f;
		if (bx < 4.f)
			bx = 4.f;
		ImGui::SetCursorPos(ImVec2(bx, ImGui::GetCursorPosY() + 1.f));
		ImGui::PushID("lua_out_clear");
		if (widgets::button("clear", ImVec2(btn_w, 0.f)))
			g_output.clear();
		ImGui::PopID();
	}

	ImGui::SetCursorPos(ImVec2(4.f, ImGui::GetCursorPosY() + 2.f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.f);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
		ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.35f));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,
		ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.55f));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, colors::accent);

	if (ImGui::BeginChild("##lua_output_scroll", ImVec2(0, 0), ImGuiChildFlags_None))
	{
		ImGui::PushFont(font, fs);
		if (g_output.empty())
		{
			widgets::draw_outlined_text(
				ImGui::GetWindowDrawList(), font, fs,
				ImGui::GetCursorScreenPos(),
				colors::text_inactive_u32(),
				"пусто, жми run");
			ImGui::Dummy(ImVec2(1.f, row_h));
		}

		else
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			for (int i = 0; i < (int)g_output.size(); ++i)
			{
				const auto& line = g_output[i];
				char prefix[16];
				std::snprintf(prefix, sizeof(prefix), "[%s] ", LevelTag(line.level));

				ImVec2 row = ImGui::GetCursorScreenPos();
				float prefix_w = font->CalcTextSizeA(fs, FLT_MAX, 0.f, prefix).x;

				widgets::draw_outlined_text(dl, font, fs, row, LevelColor(line.level), prefix);
				widgets::draw_outlined_text(
					dl, font, fs,
					ImVec2(row.x + prefix_w, row.y),
					LevelColor(line.level),
					line.text.c_str());

				float tw = font->CalcTextSizeA(fs, FLT_MAX, 0.f, line.text.c_str()).x;
				ImGui::Dummy(ImVec2(prefix_w + tw + 8.f, row_h));
			}
		}

		if (g_scroll_out)
		{
			ImGui::SetScrollHereY(1.f);
			g_scroll_out = false;
		}
		ImGui::PopFont();
	}
	ImGui::EndChild();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	widgets::end_child_panel();
}

inline void DrawStatusBar(TextEditor& ed, float width)
{
	ImFont* font = fonts::ui();
	float fs = fonts::ui_size(font) - 1.f;
	float status_h = 16.f;
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec4 bar = colors::child_fill;
	bar.x = bar.x + 0.03f;
	if (bar.x > 1.f) bar.x = 1.f;
	bar.y = bar.y + 0.03f;
	if (bar.y > 1.f) bar.y = 1.f;
	bar.z = bar.z + 0.03f;
	if (bar.z > 1.f) bar.z = 1.f;
	dl->AddRectFilled(p, ImVec2(p.x + width, p.y + status_h), ToU32(bar));

	ImVec4 line = colors::accent;
	line.w = 0.25f;
	dl->AddRectFilled(p, ImVec2(p.x + width, p.y + 1.f), ToU32(line));

	auto cur = ed.GetCursorPosition();
	char left[96];
	char right[64];
	std::snprintf(left, sizeof(left), "Ln %d, Col %d", cur.mLine + 1, cur.mColumn + 1);
	std::snprintf(right, sizeof(right), "Lua  |  %d lines", ed.GetTotalLines());

	widgets::draw_outlined_text(
		dl, font, fs,
		ImVec2(ImFloor(p.x + 6.f), ImFloor(p.y + (status_h - fs) * 0.5f)),
		colors::text_inactive_u32(), left);

	ImVec2 rs = font->CalcTextSizeA(fs, FLT_MAX, 0.f, right);
	widgets::draw_outlined_text(
		dl, font, fs,
		ImVec2(ImFloor(p.x + width - rs.x - 6.f), ImFloor(p.y + (status_h - fs) * 0.5f)),
		colors::text_inactive_u32(), right);

	ImGui::Dummy(ImVec2(width, status_h));
}

inline void DrawEditor(float width, float height)
{
	float tf = PanelFs();
	if (widgets::begin_child_panel(
			"lua_editor", ImVec2(width, height),
			"editor", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
	{
		TextEditor& ed = CurEditor();
		ed.SetPalette(MakePalette());

		ImGui::SetCursorPos(ImVec2(2.f, ImGui::GetCursorPosY() + 1.f));
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float status_h = 16.f;
		float editor_h = avail.y - status_h - 2.f;
		if (editor_h < 40.f)
			editor_h = 40.f;
		float editor_w = avail.x - 2.f;
		if (editor_w < 40.f)
			editor_w = 40.f;

		ImFont* code_font = fonts::ui();
		if (fonts::proggy_clean)
			code_font = fonts::proggy_clean;
		ImGui::PushFont(code_font, fonts::ui_size(code_font));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.f);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
			ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,
			ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, colors::accent);

		char id[32];
		std::snprintf(id, sizeof(id), "##lua_te_%d", g_tab);
		ed.Render(id, ImVec2(editor_w, editor_h), false);

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar();
		ImGui::PopFont();

		DrawStatusBar(ed, editor_w);
	}
	widgets::end_child_panel();
}

}
}
}
