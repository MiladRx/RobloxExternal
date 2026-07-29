#include "lua_ui.h"

#include "jewsploit_shell.h"
#include "widgets/float_panel.h"
#include "widgets/child.h"
#include "widgets/btn.h"
#include "colors/colors.h"

#include "features/lua/LuaExecutor.h"
#include "features/lua/State.h"
#include "features/lua/vm/LuaVM.h"
#include "features/lua/execute/Tabs.h"
#include "features/lua/execute/Scripts.h"
#include "features/lua/protocol/Log.h"
#include "app/Settings.h"
#include "gui/resources/fonts/fonts.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <string>

using namespace Cheat::Features;
using namespace Cheat::Features::LuaDetail;

namespace
{
	int edit_cb(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			std::string* s = (std::string*)data->UserData;
			s->resize((size_t)data->BufSize);
			data->Buf = s->data();
		}

		return 0;
	}

	void draw_code_box(float w, float h)
	{
		std::string& src = CurText();
		if (src.capacity() < 4096)
			src.reserve(4096);

		// imgui хочет живой буфер с нулём
		if (src.empty())
		{
			src.push_back('\0');
			src.clear();
			src.reserve(4096);
		}

		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		float rnd = 10.f;

		ImU32 bg = col::checkbox_off_u32();
		dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, rnd);
		dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(255, 255, 255, 28), rnd, 0, 1.f);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(220.f / 255.f, 226.f / 255.f, 236.f / 255.f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rnd);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

		ImGui::SetCursorScreenPos(pos);
		ImGuiInputTextFlags flags =
			ImGuiInputTextFlags_AllowTabInput |
			ImGuiInputTextFlags_CallbackResize;

		ImGui::InputTextMultiline(
			"##lua_code",
			src.data(),
			src.capacity() + 1,
			ImVec2(w, h),
			flags,
			edit_cb,
			&src
		);

		src.resize(std::strlen(src.c_str()));

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(4);
	}

	void draw_editor(float w, float h)
	{
		if (!ng::child_begin("##lua_ed", "editor", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		ImVec2 av = ImGui::GetContentRegionAvail();
		float eh = av.y - 10.f;
		if (eh < 80.f) eh = 80.f;
		float ew = av.x;
		if (ew < 40.f) ew = 40.f;

		draw_code_box(ew, eh);
		ng::child_end();
	}

	void draw_scripts(float w, float h)
	{
		if (!ng::child_begin("##lua_scr", "scripts", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		if (g_scripts.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), "no scripts");
		}

		else
		{
			for (int i = 0; i < (int)g_scripts.size(); i++)
			{
				ImGui::PushID(i);
				bool selected = false;
				if (!g_tabs.empty() &&
				    g_tabs[g_tab].path == (ScriptsDir() / g_scripts[i]).string())
				{
					selected = true;
				}

				if (ImGui::Selectable(g_scripts[i].c_str(), selected))
					LoadFileIntoTab(ScriptsDir() / g_scripts[i]);

				ImGui::PopID();
			}
		}

		ImGui::Dummy(ImVec2(0.f, 10.f));
		ng::child_end();
	}

	void draw_output(float w, float h)
	{
		if (!ng::child_begin("##lua_out", "output", w, h, 12.f, true))
		{
			ng::child_end();
			return;
		}

		if (g_output.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), "empty - press execute");
		}

		else
		{
			float wrap = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
			for (int i = 0; i < (int)g_output.size(); i++)
			{
				const auto& line = g_output[i];
				ImGui::PushID(i);

				char label[2048]{};
				std::snprintf(label, sizeof(label), "[%s]  %s",
				              LevelTag(line.level), line.text.c_str());

				ImVec4 tc = ImGui::ColorConvertU32ToFloat4(LevelColor(line.level));
				ImGui::PushStyleColor(ImGuiCol_Text, tc);
				ImGui::PushTextWrapPos(wrap);
				ImGui::TextUnformatted(label);
				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();

				ImGui::PopID();
			}
		}

		if (g_scroll_out && g_out_auto_scroll)
		{
			ImGui::SetScrollHereY(1.f);
			g_scroll_out = false;
		}

		ImGui::Dummy(ImVec2(0.f, 12.f));
		ng::child_end();
	}
}

void ng_lua::draw(float alpha)
{
	if (alpha <= 0.001f)
		return;

	// want = меню + тумблер; закрытие анимирует float_panel (флаг не жрём при hide меню)
	bool open = menu::open && Cheat::g_Settings.lua.executor;

	LuaExecutor::Initialize();

	float now = (float)ImGui::GetTime();
	if (now >= g_refresh_at)
	{
		RefreshScripts();
		g_refresh_at = now + 1.5f;
	}

	ImGui::PushFont(fonts::ui(), fonts::ui_size());
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	bool began = ng::float_panel_begin(
		"##jewsploit_lua_executor",
		"lua executor",
		&open,
		900.f, 600.f,
		640.f, 420.f
	);

	if (began)
	{
		ImVec2 wsz = ImGui::GetWindowSize();
		float head = ng::float_panel_head_h();
		float pad = 16.f;
		float gap = 12.f;
		float btn_h = 34.f;

		float avail_w = wsz.x - pad * 2.f;
		float avail_h = wsz.y - head - pad - 12.f;
		if (avail_w < 360.f) avail_w = 360.f;
		if (avail_h < 260.f) avail_h = 260.f;

		float body_h = avail_h - btn_h - gap;
		if (body_h < 180.f) body_h = 180.f;

		float side_w = 200.f;
		float left_w = avail_w - side_w - gap;
		if (left_w < 240.f) left_w = 240.f;

		// больше editor, output компактнее
		float ed_h = body_h * 0.72f;
		float out_h = body_h - ed_h - gap;
		if (ed_h < 160.f) ed_h = 160.f;
		if (out_h < 80.f)
		{
			out_h = 80.f;
			ed_h = body_h - out_h - gap;
		}

		float x = pad;
		float y = head + 12.f;

		ImGui::SetCursorPos(ImVec2(x, y));
		draw_editor(left_w, ed_h);

		ImGui::SetCursorPos(ImVec2(x + left_w + gap, y));
		draw_scripts(side_w, body_h);

		ImGui::SetCursorPos(ImVec2(x, y + ed_h + gap));
		draw_output(left_w, out_h);

		float by = y + ed_h + gap + out_h + gap;
		float bw = (avail_w - gap * 2.f) / 3.f;
		if (bw < 90.f) bw = 90.f;

		ImGui::SetCursorPos(ImVec2(x, by));
		if (ng::btn("##exec", "execute", bw, btn_h))
		{
			LuaExecutor::Log(LogLevel::Info, "running...");
			LuaVM::Execute(CurText(), Cur().name.c_str());
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##clr", "clear", bw, btn_h))
		{
			ClearCurrent();
			LuaExecutor::Log(LogLevel::Info, "editor cleared");
		}
		ImGui::SameLine(0.f, gap);
		if (ng::btn("##clr_out", "clear output", bw, btn_h))
		{
			g_output.clear();
			g_out_sel = -1;
		}

		ng::float_panel_end();
	}

	// крест — гасим тумблер; закрытие меню флаг не трогает
	if (!open && menu::open)
		Cheat::g_Settings.lua.executor = false;

	ImGui::PopStyleVar();
	ImGui::PopFont();
}
