#include "pch.h"
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "LuaExecutor.h"

#include "app/Settings.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "gui/TextEditor/TextEditor.h"

#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace fs = std::filesystem;

namespace {

//constexpr size_t k_editor_cap = 256 * 1024;
//constexpr size_t k_output_cap = 500;

using LogLevel = Cheat::Features::LuaExecutor::LogLevel;

struct EditorTab {
    std::string name;
    std::string path;
    std::unique_ptr<TextEditor> editor;
};

struct OutputLine {
    LogLevel level = LogLevel::Info;
    std::string text;
};

std::vector<EditorTab> g_tabs;
int g_tab = 0;
std::vector<std::string> g_scripts;
std::vector<OutputLine> g_output;
bool g_scroll_out = false;
float g_refresh_at = 0.f;
bool g_inited = false;

const char* LevelTag(LogLevel level)
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

ImU32 LevelColor(LogLevel level)
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

void PushOutput(LogLevel level, const char* text)
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

float PanelFs()
{
    return fonts::ui_size(fonts::ui_bold());
}

ImU32 ToU32(const ImVec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 MixU32(const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
{
	ImVec4 m;
	m.x = a.x + (b.x - a.x) * t;
	m.y = a.y + (b.y - a.y) * t;
	m.z = a.z + (b.z - a.z) * t;
	m.w = alpha;
	return ToU32(m);
}

TextEditor::Palette MakePalette()
{
	/* editor colors */
	TextEditor::Palette p = TextEditor::GetDarkPalette();

	ImVec4 bg = colors::child_fill;
	ImVec4 text = colors::text_active;
	ImVec4 dim = colors::text_inactive;
	ImVec4 acc = colors::accent;

	p[(int)TextEditor::PaletteIndex::Default] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::Keyword] = ToU32(acc);
	p[(int)TextEditor::PaletteIndex::Number] = MixU32(acc, ImVec4(0.55f, 0.95f, 0.70f, 1.f), 0.55f);
	p[(int)TextEditor::PaletteIndex::String] = MixU32(acc, ImVec4(0.95f, 0.75f, 0.45f, 1.f), 0.65f);
	p[(int)TextEditor::PaletteIndex::CharLiteral] = MixU32(acc, ImVec4(0.95f, 0.75f, 0.45f, 1.f), 0.45f);
	p[(int)TextEditor::PaletteIndex::Punctuation] = MixU32(text, dim, 0.25f);
	p[(int)TextEditor::PaletteIndex::Preprocessor] = MixU32(acc, ImVec4(0.70f, 0.55f, 0.95f, 1.f), 0.50f);
	p[(int)TextEditor::PaletteIndex::Identifier] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::KnownIdentifier] = MixU32(acc, text, 0.35f);
	p[(int)TextEditor::PaletteIndex::PreprocIdentifier] = MixU32(acc, ImVec4(0.85f, 0.55f, 0.90f, 1.f), 0.40f);
	p[(int)TextEditor::PaletteIndex::Comment] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.85f));
	p[(int)TextEditor::PaletteIndex::MultiLineComment] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.75f));
	p[(int)TextEditor::PaletteIndex::Background] = ToU32(bg);
	p[(int)TextEditor::PaletteIndex::Cursor] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::Selection] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.35f));
	p[(int)TextEditor::PaletteIndex::ErrorMarker] = IM_COL32(255, 60, 60, 140);
	p[(int)TextEditor::PaletteIndex::Breakpoint] = IM_COL32(255, 140, 40, 90);
	p[(int)TextEditor::PaletteIndex::LineNumber] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.70f));
	p[(int)TextEditor::PaletteIndex::CurrentLineFill] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.08f));
	p[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = ToU32(ImVec4(1.f, 1.f, 1.f, 0.03f));
	p[(int)TextEditor::PaletteIndex::CurrentLineEdge] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.35f));
	return p;
}

void ConfigureEditor(TextEditor& ed)
{
	ed.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
	ed.SetPalette(MakePalette());
	ed.SetShowWhitespaces(false);
	ed.SetTabSize(4);
	ed.SetHandleKeyboardInputs(true);
	ed.SetHandleMouseInputs(true);
}

std::unique_ptr<TextEditor> MakeEditor(const char* seed = "")
{
	auto ed = std::make_unique<TextEditor>();
	ConfigureEditor(*ed);
	if (seed && seed[0])
		ed->SetText(seed);
	return ed;
}

void PushChrome()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImVec4 grip = colors::child_fill;
	grip.w = 0.35f;
	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, grip);
	ImVec4 a = colors::accent;
	a.w = 0.7f;
	ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, a);
	a.w = 1.f;
	ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, a);
}

void PopChrome()
{
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
}

fs::path ScriptsDir()
{
	wchar_t mod[MAX_PATH]{};
	GetModuleFileNameW(nullptr, mod, MAX_PATH);
	fs::path dir = fs::path(mod).parent_path() / "scripts";
	std::error_code ec;
	fs::create_directories(dir, ec);
	return dir;
}

void EnsureDefaultTab()
{
	if (!g_tabs.empty())
		return;

	EditorTab t;
	t.name = "main";
	t.editor = MakeEditor("-- jewsploit lua\nprint(\"hello\")\n");
	g_tabs.push_back(std::move(t));
	g_tab = 0;
}

void RefreshScripts()
{
	g_scripts.clear();
	std::error_code ec;
	for (auto& e : fs::directory_iterator(ScriptsDir(), ec))
	{
		if (!e.is_regular_file(ec))
			continue;

		auto ext = e.path().extension().string();
		if (_stricmp(ext.c_str(), ".lua") != 0 && _stricmp(ext.c_str(), ".txt") != 0)
			continue;

		g_scripts.push_back(e.path().filename().string());
	}
	std::sort(g_scripts.begin(), g_scripts.end());
}

EditorTab& Cur()
{
    EnsureDefaultTab();
    if (g_tab < 0 || g_tab >= (int)g_tabs.size())
        g_tab = 0;
    return g_tabs[g_tab];
}

TextEditor& CurEditor()
{
    auto& t = Cur();
    if (!t.editor)
        t.editor = MakeEditor();
    return *t.editor;
}

void NewBlankTab()
{
	EditorTab t;
	int n = 1;
	for (;;)
	{
		char name[32];
		std::snprintf(name, sizeof(name), "tab %d", n);
		bool used = false;
		for (const auto& e : g_tabs)
		{
			if (e.name == name)
			{
				used = true;
				break;
			}
		}

		if (!used)
		{
			t.name = name;
			break;
		}

		++n;
	}

	t.editor = MakeEditor();
	g_tabs.push_back(std::move(t));
	g_tab = (int)g_tabs.size() - 1;
}

void CloseTab(int idx)
{
	if (idx < 0 || idx >= (int)g_tabs.size())
		return;

	// последний таб не убиваем, просто чистим
	if (g_tabs.size() == 1)
	{
		auto& t = g_tabs[0];
		t.editor = MakeEditor();
		t.path.clear();
		t.name = "main";
		g_tab = 0;
		return;
	}

	g_tabs.erase(g_tabs.begin() + idx);
	if (g_tab >= (int)g_tabs.size())
		g_tab = (int)g_tabs.size() - 1;

	else if (g_tab > idx)
		--g_tab;
}

bool LoadFileIntoTab(const fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		PushOutput(LogLevel::Error, ("failed to open " + path.filename().string()).c_str());
		return false;
	}

	std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	// 256kb хватит
	if (data.size() > 256 * 1024)
		data.resize(256 * 1024);

	for (int i = 0; i < (int)g_tabs.size(); ++i)
	{
		if (g_tabs[i].path == path.string())
		{
			g_tab = i;
			if (!g_tabs[i].editor)
				g_tabs[i].editor = MakeEditor();
			ConfigureEditor(*g_tabs[i].editor);
			g_tabs[i].editor->SetText(data);
			PushOutput(LogLevel::Info, ("opened " + path.filename().string()).c_str());
			return true;
		}
	}

	EditorTab t;
	t.name = path.stem().string();
	t.path = path.string();
	t.editor = MakeEditor();
	t.editor->SetText(data);
	g_tabs.push_back(std::move(t));
	g_tab = (int)g_tabs.size() - 1;
	PushOutput(LogLevel::Info, ("opened " + path.filename().string()).c_str());
	return true;
}

bool SaveCurrent()
{
	auto& t = Cur();
	TextEditor& ed = CurEditor();

	fs::path path;
	if (t.path.empty())
		path = ScriptsDir() / (t.name + ".lua");

	else
		path = fs::path(t.path);

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;

	std::string text = ed.GetText();
	out.write(text.data(), (std::streamsize)text.size());
	t.path = path.string();
	t.name = path.stem().string();
	RefreshScripts();
	return true;
}

void OpenScriptsFolder()
{
    ShellExecuteW(nullptr, L"open", ScriptsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CopyCurrent()
{
	std::string text = CurEditor().GetText();
	if (text.empty() || !OpenClipboard(nullptr))
		return;

	EmptyClipboard();
	size_t n = text.size() + 1;
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
	if (mem)
	{
		void* p = GlobalLock(mem);
		if (p)
		{
			std::memcpy(p, text.c_str(), n);
			GlobalUnlock(mem);
			SetClipboardData(CF_TEXT, mem);
		}
	}
	CloseClipboard();
}

void ClearCurrent()
{
    CurEditor().SetText("");
}

bool DrawHeaderClose(const ImVec2& panel_min, float panel_w)
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

void DrawTabStrip(float avail_w)
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

void DrawScriptList(float width, float height)
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

void DrawOutput(float width, float height)
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

void DrawStatusBar(TextEditor& ed, float width)
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

void DrawEditor(float width, float height)
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

namespace Cheat::Features {

void LuaExecutor::Initialize()
{
	if (g_inited)
		return;

	g_inited = true;
	EnsureDefaultTab();
	RefreshScripts();
}

void LuaExecutor::Shutdown()
{
	g_tabs.clear();
	g_scripts.clear();
	g_output.clear();
	g_tab = 0;
	g_inited = false;
}

void LuaExecutor::ClearOutput()
{
	g_output.clear();
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
	PushOutput(level, buf);
}

void LuaExecutor::Render(float alpha)
{
	if (!g_Settings.lua.executor)
		return;
	if (alpha <= 0.001f)
		return;

	Initialize();

	float now = (float)ImGui::GetTime();
	if (now >= g_refresh_at)
	{
		RefreshScripts();
		g_refresh_at = now + 1.5f;
	}

	ImGui::SetNextWindowSize(ImVec2(760.f, 500.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 340.f), ImVec2(1600.f, 1100.f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
	PushChrome();

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
				"lua", fonts::ui_bold(), PanelFs(),
				nullptr, nullptr, nullptr))
		{
			DrawTabStrip(ImGui::GetContentRegionAvail().x - 8.f);

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
			DrawEditor(editor_w, editor_h);
			ImGui::SetCursorPos(ImVec2(6.f, body_y + editor_h + gap));
			DrawOutput(editor_w, output_h);

			ImGui::SetCursorPos(ImVec2(6.f + editor_w + gap, body_y));
			DrawScriptList(side, stack_h);

			float btn_y = body_y + stack_h + 4.f;
			float btn_gap = 4.f;
			float left_btn_w = (editor_w - btn_gap * 3.f) / 4.f;

			ImGui::SetCursorPos(ImVec2(6.f, btn_y));
			if (widgets::button("run", ImVec2(left_btn_w, 0.f)))
			{
				(void)CurEditor().GetText();
				Log(LogLevel::Info, "running...");
				Log(LogLevel::Warn, "lua vm stub пока нет");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("copy", ImVec2(left_btn_w, 0.f)))
			{
				CopyCurrent();
				Log(LogLevel::Info, "copied to clipboard");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("save", ImVec2(left_btn_w, 0.f)))
			{
				if (SaveCurrent())
					Log(LogLevel::Success, "saved %s", Cur().name.c_str());

				else
					Log(LogLevel::Error, "failed to save script");
			}
			ImGui::SameLine(0.f, btn_gap);
			if (widgets::button("clear", ImVec2(left_btn_w, 0.f)))
			{
				ClearCurrent();
				Log(LogLevel::Info, "editor cleared");
			}

			ImGui::SetCursorPos(ImVec2(6.f + editor_w + gap, btn_y));
			if (widgets::button("open folder", ImVec2(side, 0.f)))
			{
				OpenScriptsFolder();
			}
		}
		widgets::end_child_panel();

		if (DrawHeaderClose(panel_min, panel_w))
			g_Settings.lua.executor = false;
	}
	ImGui::End();
	PopChrome();
	ImGui::PopStyleVar();
}

}
