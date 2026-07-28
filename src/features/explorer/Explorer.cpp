#include "pch.h"
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Explorer.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "app/Graphics.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <stb_image.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <Windows.h>

#include "gui/resources/icons/iconsdex.h"

#undef GetClassName

namespace {

#include "ExplorerDraw.h"
#include "ExplorerTree.h"
#include "ExplorerSearch.h"

}

namespace Cheat::Features {

void Explorer::Initialize()
{
	if (g_icons_ok)
		return;
	if (!Cheat::Core::g_Device)
		return;

	g_icons_ok = true;

	RegisterIcon("workspace", workspace, (int)sizeof(workspace));
    RegisterIcon("folder", folder, (int)sizeof(folder));
    RegisterIcon("camera", camera, (int)sizeof(camera));
    RegisterIcon("lightning", lightning, (int)sizeof(lightning));
    RegisterIcon("humanoid", humanoid, (int)sizeof(humanoid));
    RegisterIcon("part", part, (int)sizeof(part));
    RegisterIcon("players", players, (int)sizeof(players));
    RegisterIcon("meshpart", meshpart, (int)sizeof(meshpart));
    RegisterIcon("player", player, (int)sizeof(player));
    RegisterIcon("model", model, (int)sizeof(model));
    RegisterIcon("terrain", terrain, (int)sizeof(terrain));
    RegisterIcon("localscript", localscript, (int)sizeof(localscript));
    RegisterIcon("localscripts", localscripts, (int)sizeof(localscripts));
    RegisterIcon("playergui", playergui, (int)sizeof(playergui));
    RegisterIcon("stats", stats, (int)sizeof(stats));
    RegisterIcon("guiservice", guiservice, (int)sizeof(guiservice));
    RegisterIcon("videocapture", videocapture, (int)sizeof(videocapture));
    RegisterIcon("runservice", runservice, (int)sizeof(runservice));
    RegisterIcon("frame", frame, (int)sizeof(frame));
    RegisterIcon("csd", csd, (int)sizeof(csd));
    RegisterIcon("contentprovider", contentprovider, (int)sizeof(contentprovider));
    RegisterIcon("nonreplicated", nonreplicated, (int)sizeof(nonreplicated));
    RegisterIcon("startergear", startergear, (int)sizeof(startergear));
    RegisterIcon("timerdevice", timerdevice, (int)sizeof(timerdevice));
    RegisterIcon("backpack", backpack, (int)sizeof(backpack));
    RegisterIcon("marketplaceservice", marketplaceservice, (int)sizeof(marketplaceservice));
    RegisterIcon("soundservice", soundservice, (int)sizeof(soundservice));
    RegisterIcon("logservice", logservice, (int)sizeof(logservice));
    RegisterIcon("statsitem", statsitem, (int)sizeof(statsitem));
    RegisterIcon("boolvalue", boolvalue, (int)sizeof(boolvalue));
    RegisterIcon("intvalue", intvalue, (int)sizeof(intvalue));
    RegisterIcon("doubletype", doubletype, (int)sizeof(doubletype));
    RegisterIcon("typeshit", typeshit, (int)sizeof(typeshit));
}

void Explorer::Shutdown()
{
	StopSearch();
	for (auto& kv : g_icons)
	{
		if (kv.second.srv)
			kv.second.srv->Release();
	}
	g_icons.clear();
	g_icons_ok = false;
}

void Explorer::Render(float alpha)
{
	if (!Cheat::g_Settings.misc.explorer)
		return;
	if (alpha <= 0.001f)
		return;

	Initialize();
	EnsureRoot();

	ImGui::SetNextWindowSize(ImVec2(360.f, 460.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(260.f, 220.f), ImVec2(1000.f, 1000.f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
	PushWindowChrome();

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

	bool open = true;
	if (ImGui::Begin("##jewsploit_explorer", &open, flags))
	{
		colors::draw_panel_background(alpha);

		float margin = 10.f;
		ImVec2 sz = ImGui::GetWindowSize();
		ImGui::SetCursorPos(ImVec2(margin, margin));

		if (widgets::begin_child_panel(
				"explorer_child",
				ImVec2(sz.x - margin * 2.f, sz.y - margin * 2.f),
				"explorer", fonts::ui_bold(), PanelFontSize(),
				nullptr, nullptr, nullptr))
		{
			static char query[128] = "";

			ImGui::SetCursorPos(ImVec2(6.f, ImGui::GetCursorPosY() + 4.f));
			float search_w = ImGui::GetContentRegionAvail().x - 6.f;
			bool entered = widgets::input_text("##explorer_search", "search...",
				query, IM_ARRAYSIZE(query), search_w, ImGuiInputTextFlags_EnterReturnsTrue);

			if (entered && query[0])
				StartSearch(query);
			if (g_search_on && query[0] == '\0')
				StopSearch();

			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
			if (ImGui::BeginChild("##explorer_scroll",
					ImVec2(0.f, 0.f), false, ImGuiWindowFlags_NoScrollbar))
			{
				if (g_search_on)
				{
					std::lock_guard<std::mutex> lk(g_search_mtx);
					if (g_searching.load())
					{
						ImGui::SetCursorPosX(6.f);
						TextLine(colors::text_inactive_u32(), "searching...");
					}

					if (g_search_results.empty() && !g_searching.load())
					{
						ImGui::SetCursorPosX(6.f);
						TextLine(colors::text_inactive_u32(), "no matches");
					}

					float row_h = 18.f;
					float icon_sz = 14.f;
					ImGuiListClipper clipper;
					clipper.Begin((int)g_search_results.size(), row_h);
					while (clipper.Step())
					{
						for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
						{
							const SearchResult& r = g_search_results[i];
							ImGui::PushID(i);
							ImVec2 row_start = ImGui::GetCursorScreenPos();
							ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
								ImVec4(51/255.f, 122/255.f, 231/255.f, 0.25f));
							bool clicked = ImGui::Selectable("##sr", false,
								ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.f, row_h));
							ImGui::PopStyleColor();
							bool rc = ImGui::IsItemClicked(ImGuiMouseButton_Right);

							ImDrawList* dl = ImGui::GetWindowDrawList();
							int iw, ih;
							ID3D11ShaderResourceView* srv = IconSrv(r.cls, iw, ih);
							ImVec2 ico_min(row_start.x + 4.f,
								row_start.y + (row_h - icon_sz) * 0.5f);
							if (srv)
								dl->AddImage((ImTextureID)(uintptr_t)srv, ico_min,
									ImVec2(ico_min.x + icon_sz, ico_min.y + icon_sz));

							ImFont* font = fonts::ui();
							float fs = PanelFontSize();
							const char* label = "?";
							if (!r.name.empty())
								label = r.name.c_str();

							widgets::draw_outlined_text(dl, font, fs,
								ImVec2(ico_min.x + icon_sz + 4.f,
									ImFloor(row_start.y + (row_h - fs) * 0.5f)),
								IM_COL32(220, 220, 224, 255),
								label);
							ImGui::SetCursorScreenPos(ImVec2(row_start.x, row_start.y + row_h));

							if (clicked || rc)
							{
								g_ctx.address = r.address;
								g_ctx.name = r.name;
								g_ctx.cls = r.cls;
								g_ctx.path = r.path;
								g_ctx.valid = true;
								g_open_ctx = true;
							}
							ImGui::PopID();
						}
					}
					clipper.End();
				}

				else
				{
					if (g_root.address)
					{
						if (!g_root.loaded)
							LoadChildren(g_root);
						for (auto& child : g_root.children)
							RenderNode(child, 0);
					}

					else
					{
						ImGui::SetCursorPosX(6.f);
						TextLine(colors::text_inactive_u32(), "not attached / no datamodel");
					}
				}

				DrawContextMenu();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
		widgets::end_child_panel();
	}
	ImGui::End();
	PopWindowChrome();
	ImGui::PopStyleVar();

	if (!open)
		Cheat::g_Settings.misc.explorer = false;

	DrawDetailWindow(alpha);
}

}
