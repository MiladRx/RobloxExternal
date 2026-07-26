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

struct IconTex {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 16, h = 16;
};

std::unordered_map<std::string, IconTex> g_icons;
bool g_icons_ok = false;

ID3D11ShaderResourceView* DecodePng(const unsigned char* png, int len, int& out_w, int& out_h)
{
	if (!Cheat::Core::g_Device)
		return nullptr;

	int ch = 0;
	unsigned char* pixels = stbi_load_from_memory(png, len, &out_w, &out_h, &ch, 4);
	if (!pixels)
		return nullptr;

	D3D11_TEXTURE2D_DESC td{};
	td.Width = out_w;
	td.Height = out_h;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = pixels;
	sd.SysMemPitch = out_w * 4;

	ID3D11Texture2D* tex = nullptr;
	HRESULT hr = Cheat::Core::g_Device->CreateTexture2D(&td, &sd, &tex);
	stbi_image_free(pixels);
	if (FAILED(hr) || !tex)
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = Cheat::Core::g_Device->CreateShaderResourceView(tex, nullptr, &srv);
	tex->Release();

	if (FAILED(hr))
		return nullptr;

	return srv;
}

void RegisterIcon(const char* key, const unsigned char* png, int len)
{
	IconTex it;
	it.srv = DecodePng(png, len, it.w, it.h);
	if (it.srv)
		g_icons[key] = it;
}

const char* IconKeyForClass(const std::string& cls)
{
	if (cls == "LocalScript" || cls == "Script" || cls == "ModuleScript")
		return "localscript";

	if (cls == "Part" || cls == "TrussPart" || cls == "WedgePart" ||
		cls == "CornerWedgePart" || cls == "Seat" || cls == "VehicleSeat" ||
		cls == "SpawnLocation" || cls == "UnionOperation" ||
		cls == "NegateOperation" || cls == "IntersectOperation")
	{
		return "part";
	}

	if (cls == "MeshPart")
		return "meshpart";
	if (cls == "Terrain")
		return "terrain";

	if (cls == "Workspace")
		return "workspace";
	if (cls == "Folder" || cls == "Configuration")
		return "folder";
	if (cls == "Model" || cls == "Actor" || cls == "WorldModel")
		return "model";

	if (cls == "Camera")
		return "camera";
	if (cls == "Lighting")
		return "lightning";
	if (cls == "Players")
		return "players";
	if (cls == "Player")
		return "player";
	if (cls == "Humanoid")
		return "humanoid";
	if (cls == "Backpack")
		return "backpack";
	if (cls == "StarterGear")
		return "startergear";
	if (cls == "Stats")
		return "stats";
	if (cls == "StatsItem")
		return "statsitem";
	if (cls == "GuiService")
		return "guiservice";
	if (cls == "RunService")
		return "runservice";
	if (cls == "LogService")
		return "logservice";
	if (cls == "SoundService" || cls == "Sound")
		return "soundservice";
	if (cls == "MarketplaceService")
		return "marketplaceservice";
	if (cls == "ContentProvider")
		return "contentprovider";
	if (cls == "VideoCaptureService")
		return "videocapture";

	if (cls == "PlayerGui" || cls == "StarterGui" || cls == "ScreenGui" ||
		cls == "CoreGui")
	{
		return "playergui";
	}

	if (cls == "Frame" || cls == "TextLabel" || cls == "TextButton" ||
		cls == "ImageLabel" || cls == "ImageButton" || cls == "TextBox")
	{
		return "frame";
	}

	if (cls == "BoolValue")
		return "boolvalue";
	if (cls == "IntValue")
		return "intvalue";
	if (cls == "NumberValue" || cls == "DoubleConstrainedValue")
		return "doubletype";

	return "typeshit";
}

ID3D11ShaderResourceView* IconSrv(const std::string& cls, int& w, int& h)
{
	const char* key = IconKeyForClass(cls);
	auto it = g_icons.find(key);
	if (it == g_icons.end())
		it = g_icons.find("typeshit");

	if (it == g_icons.end())
	{
		w = 16;
		h = 16;
		return nullptr;
	}

	w = it->second.w;
	h = it->second.h;
	return it->second.srv;
}

bool IsScriptClass(const std::string& cls)
{
	return cls == "LocalScript" || cls == "Script" || cls == "ModuleScript";
}

struct Node {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::vector<Node> children;
    bool loaded = false;
    bool open = false;
};

Node g_root;
std::uint64_t g_root_addr = 0;

//constexpr int k_child_cap = 800;

void LoadChildren(Node& n)
{
	if (n.loaded)
		return;

	n.loaded = true;
	n.children.clear();

	Cheat::Instance inst(n.address);
	for (const auto& c : inst.GetChildren())
	{
		Node cn;
		cn.address = c.address;
		cn.name = c.GetName();
		cn.cls = c.GetClassName();
		n.children.push_back(std::move(cn));
	}
}

void EnsureRoot()
{
	std::uint64_t dm = Cheat::Globals::InstanceDataModel.address;
	if (dm && dm != g_root_addr)
	{
		g_root_addr = dm;
		g_root = Node{};
		g_root.address = dm;
		g_root.cls = "DataModel";
		g_root.name = "game";
		g_root.open = true;
	}
}

struct SearchResult {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::string path;
};

std::mutex g_search_mtx;
std::vector<SearchResult> g_search_results;
std::atomic<std::uint32_t> g_search_gen{ 0 };
std::atomic<bool> g_searching{ false };
bool g_search_on = false;

std::string ToLower(std::string s)
{
	for (char& ch : s)
		ch = (char)std::tolower((unsigned char)ch);
	return s;
}

void RunSearch(std::string query, std::uint64_t root_addr, std::uint32_t gen)
{
	std::string q = ToLower(query);
	std::vector<SearchResult> local;

	struct Frame
	{
		std::uint64_t addr;
		std::string path;
	};
	std::vector<Frame> stack;
	stack.push_back({ root_addr, "game" });

	// 400k нод / 1k результатов а то вечность
	int budget = 400000;
	while (!stack.empty())
	{
		if (g_search_gen.load() != gen)
			return;

		if (budget-- <= 0)
			break;

		Frame f = std::move(stack.back());
		stack.pop_back();

		Cheat::Instance inst(f.addr);
		for (const auto& child : inst.GetChildren())
		{
			if (g_search_gen.load() != gen)
				return;

			std::string name = child.GetName();
			std::string cls = child.GetClassName();

			if (ToLower(name).find(q) != std::string::npos ||
				ToLower(cls).find(q) != std::string::npos)
			{
				if ((int)local.size() < 1000)
				{
					SearchResult r;
					r.address = child.address;
					r.name = name;
					r.cls = cls;
					r.path = f.path + "." + name;
					local.push_back(std::move(r));
				}
			}

			if ((int)stack.size() < 400000)
				stack.push_back({ child.address, f.path + "." + name });
		}
	}

	if (g_search_gen.load() != gen)
		return;

	{
		std::lock_guard<std::mutex> lk(g_search_mtx);
		g_search_results = std::move(local);
	}
	g_searching.store(false);
}

void StartSearch(const char* query)
{
	std::uint32_t gen = ++g_search_gen;
	g_searching.store(true);
	g_search_on = true;
	{
		std::lock_guard<std::mutex> lk(g_search_mtx);
		g_search_results.clear();
	}

	if (!g_root_addr)
	{
		g_searching.store(false);
		return;
	}

	std::thread(RunSearch, std::string(query), g_root_addr, gen).detach();
}

void StopSearch()
{
	++g_search_gen;
	g_searching.store(false);
	g_search_on = false;
	std::lock_guard<std::mutex> lk(g_search_mtx);
	g_search_results.clear();
}

struct Target {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::string path;
    bool valid = false;
};

Target g_ctx;
Target g_detail;
bool g_detail_open = false;
bool g_open_ctx = false;

std::vector<unsigned char> g_bc_bytes;
std::uint64_t g_bc_for = 0;
std::string g_bc_status;

std::string BuildPath(std::uint64_t address)
{
	std::vector<std::string> parts;
	std::uint64_t cur = address;
	int guard = 64;

	while (cur && guard-- > 0)
	{
		Cheat::Instance node(cur);
		std::string nm = node.GetName();
		if (nm.empty())
			nm = "?";
		parts.push_back(nm);

		if (cur == g_root_addr)
			break;

		auto parent = node.GetParent();
		if (!parent)
			break;

		cur = parent->address;
	}

	std::string out;
	for (auto it = parts.rbegin(); it != parts.rend(); ++it)
	{
		if (!out.empty())
			out += ".";
		out += *it;
	}

	return out;
}

void DumpBytecode(const Target& t)
{
	g_bc_bytes.clear();
	g_bc_for = t.address;
	g_bc_status.clear();

	uintptr_t bc_off = Offsets::LocalScript::ByteCode;
	if (t.cls == "ModuleScript")
		bc_off = Offsets::ModuleScript::ByteCode;

	std::uint64_t bc_obj = g_Memory.Read<std::uint64_t>(t.address + bc_off);
	if (!g_Memory.IsValid(bc_obj))
	{
		g_bc_status = "no bytecode object";
		return;
	}

	std::uint64_t ptr = g_Memory.Read<std::uint64_t>(bc_obj + Offsets::ByteCode::Pointer);
	std::uint64_t size = g_Memory.Read<std::uint64_t>(bc_obj + Offsets::ByteCode::Size);

	// 16mb потолок, иначе хз что это
	if (!g_Memory.IsValid(ptr) || size == 0 || size > (16u * 1024u * 1024u))
	{
		g_bc_status = "empty / unreadable bytecode";
		return;
	}

	g_bc_bytes.resize((size_t)size);
	SIZE_T got = g_Memory.ReadRaw(ptr, g_bc_bytes.data(), (SIZE_T)size);
	g_bc_bytes.resize(got);

	char buf[96];
	std::snprintf(buf, sizeof(buf), "%zu bytes (compressed Luau bytecode)", (size_t)got);
	g_bc_status = buf;
}

void SaveBytecodeToFile(const Target& t)
{
	if (g_bc_bytes.empty())
		return;

	std::string safe;
	for (char c : t.name)
	{
		if (std::isalnum((unsigned char)c))
			safe += c;

		else
			safe += '_';
	}

	if (safe.empty())
		safe = "script";

	std::string fname = safe + ".luauc";
	std::ofstream f(fname, std::ios::binary);
	if (f)
	{
		f.write((const char*)g_bc_bytes.data(), (std::streamsize)g_bc_bytes.size());
		g_bc_status = "saved: " + fname;
	}

	else
	{
		g_bc_status = "failed to write file";
	}
}

float PanelFontSize()
{
	return fonts::ui_size();
}

void PushWindowChrome()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.15f, 0.15f, 0.15f, 0.35f));
	ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(51/255.f, 122/255.f, 231/255.f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, ImVec4(51/255.f, 122/255.f, 231/255.f, 1.f));
}

void PopWindowChrome()
{
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
}

void DrawFramedBox(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max)
{
	ImVec2 inner_min(min.x + 1.f, min.y + 1.f);
	ImVec2 inner_max(max.x - 1.f, max.y - 1.f);
	ImVec2 fill_min(inner_min.x + 1.f, inner_min.y + 1.f);
	ImVec2 fill_max(inner_max.x - 1.f, inner_max.y - 1.f);

	draw_list->AddRectFilled(fill_min, fill_max, colors::widget_track_u32());
	draw_list->AddRect(inner_min, inner_max, colors::widget_inline_u32(), 0.f, 0, 1.f);
	draw_list->AddRect(min, max, colors::widget_outline_u32(), 0.f, 0, 1.f);
}

void TextLine(ImU32 color, const char* fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	ImFont* font = fonts::ui();
	float fs = PanelFontSize();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	widgets::draw_outlined_text(dl, font, fs, ImVec2(ImFloor(pos.x), ImFloor(pos.y)), color, buf);
	ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, buf);
	ImGui::Dummy(ImVec2(sz.x, sz.y + 3.f));
}

void DrawRowVisuals(const Node& n, float indent, bool show_arrow, bool open)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();

	float row_h = 18.f;
	float icon_sz = 14.f;
	float x = p.x + indent;

	if (show_arrow)
	{
		ImU32 acol = IM_COL32(150, 150, 155, 255);
		float cy = p.y + row_h * 0.5f;
		if (open)
		{
			dl->AddTriangleFilled(ImVec2(x, cy - 2.f), ImVec2(x + 7.f, cy - 2.f),
				ImVec2(x + 3.5f, cy + 3.f), acol);
		}

		else
		{
			dl->AddTriangleFilled(ImVec2(x + 1.f, cy - 4.f), ImVec2(x + 6.f, cy),
				ImVec2(x + 1.f, cy + 4.f), acol);
		}
	}
	x += 11.f;

	int iw = 16, ih = 16;
	ID3D11ShaderResourceView* srv = IconSrv(n.cls, iw, ih);
	ImVec2 ico_min(x, p.y + (row_h - icon_sz) * 0.5f);
	ImVec2 ico_max(ico_min.x + icon_sz, ico_min.y + icon_sz);
	if (srv)
		dl->AddImage((ImTextureID)(uintptr_t)srv, ico_min, ico_max);
	x += icon_sz + 4.f;

	ImFont* font = fonts::ui();
	float fs = PanelFontSize();
	const char* label = "?";
	if (!n.name.empty())
		label = n.name.c_str();
	ImVec2 tpos(ImFloor(x), ImFloor(p.y + (row_h - fs) * 0.5f));
	widgets::draw_outlined_text(dl, font, fs, tpos, IM_COL32(220, 220, 224, 255), label);
}

void RenderNode(Node& n, int depth)
{
	ImGui::PushID((void*)(uintptr_t)n.address);

	float row_h = 18.f;
	float indent = depth * 13.f + 2.0f;
	bool show_arrow = (!n.loaded) || (!n.children.empty());

	ImVec2 row_start = ImGui::GetCursorScreenPos();
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(51/255.f, 122/255.f, 231/255.f, 0.25f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(51/255.f, 122/255.f, 231/255.f, 0.40f));
	bool clicked = ImGui::Selectable("##row", false,
		ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, row_h));
	ImGui::PopStyleColor(2);

	bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

	ImGui::SetCursorScreenPos(row_start);
	DrawRowVisuals(n, indent, show_arrow, n.open);
	ImGui::SetCursorScreenPos(ImVec2(row_start.x, row_start.y + row_h));

	if (clicked)
	{
		if (show_arrow)
		{
			n.open = !n.open;
			if (n.open)
				LoadChildren(n);
		}
	}

	if (right_clicked)
	{
		g_ctx.address = n.address;
		g_ctx.name = n.name;
		g_ctx.cls = n.cls;
		g_ctx.path = BuildPath(n.address);
		g_ctx.valid = true;
		g_open_ctx = true;
	}

	if (n.open && n.loaded)
	{
		int count = (int)n.children.size();
		int shown = count;
		if (shown > 800)
			shown = 800;

		for (int i = 0; i < shown; ++i)
			RenderNode(n.children[i], depth + 1);

		if (count > shown)
		{
			ImGui::SetCursorPosX(indent + 20.0f);
			TextLine(colors::text_inactive_u32(), "... %d more (use search)", count - shown);
		}
	}

	ImGui::PopID();
}

void DrawContextMenu()
{
	if (g_open_ctx)
	{
		ImGui::OpenPopup("##explorer_ctx");
		g_open_ctx = false;
	}

	float item_h = 16.f;
	float pad_x = 5.f;
	float menu_w = 150.f;
	float header_h = 34.f;

	enum Action { kProps, kBytecode, kCopyName, kCopyClass, kCopyPath, kCopyAddr };
	struct Item { const char* label; Action action; };

	bool is_script = IsScriptClass(g_ctx.cls);
	Item items[6];
	int item_count = 0;
	items[item_count++] = { "properties", kProps };
	if (is_script)
		items[item_count++] = { "view bytecode", kBytecode };
	items[item_count++] = { "copy name", kCopyName };
	items[item_count++] = { "copy class", kCopyClass };
	items[item_count++] = { "copy path", kCopyPath };
	items[item_count++] = { "copy address", kCopyAddr };

	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.f);

	ImGui::SetNextWindowSize(ImVec2(menu_w, header_h + item_count * item_h + 3.f));
	if (ImGui::BeginPopup("##explorer_ctx", ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 win_min = ImGui::GetWindowPos();
		ImVec2 win_max(win_min.x + ImGui::GetWindowWidth(),
			win_min.y + ImGui::GetWindowHeight());
		DrawFramedBox(dl, win_min, win_max);

		ImFont* font = fonts::ui();
		float fs = PanelFontSize();

		const char* title = "?";
		if (!g_ctx.name.empty())
			title = g_ctx.name.c_str();

		widgets::draw_outlined_text(dl, font, fs,
			ImVec2(ImFloor(win_min.x + pad_x), ImFloor(win_min.y + 3.f)),
			colors::accent_u32(),
			title);
		widgets::draw_outlined_text(dl, font, fs,
			ImVec2(ImFloor(win_min.x + pad_x), ImFloor(win_min.y + 3.f + fs + 2.f)),
			colors::text_inactive_u32(),
			g_ctx.cls.c_str());
		dl->AddLine(ImVec2(win_min.x + 2.f, win_min.y + header_h - 1.f),
			ImVec2(win_max.x - 2.f, win_min.y + header_h - 1.f),
			colors::widget_inline_u32());

		for (int i = 0; i < item_count; ++i)
		{
			ImGui::SetCursorPos(ImVec2(0.f, header_h + i * item_h));
			char btn_id[32];
			std::snprintf(btn_id, sizeof(btn_id), "##ctx_%d", i);
			bool clicked = ImGui::InvisibleButton(btn_id, ImVec2(menu_w, item_h));
			bool hovered = ImGui::IsItemHovered();
			ImVec2 item_min = ImGui::GetItemRectMin();

			if (hovered)
			{
				dl->AddRectFilled(ImVec2(item_min.x + 2.f, item_min.y),
					ImVec2(item_min.x + menu_w - 2.f, item_min.y + item_h),
					colors::widget_track_hover_u32());
			}

			ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, items[i].label);
			ImU32 tcol = colors::text_inactive_u32();
			if (hovered)
				tcol = colors::text_active_u32();

			widgets::draw_outlined_text(dl, font, fs,
				ImVec2(ImFloor(item_min.x + pad_x),
					ImFloor(item_min.y + (item_h - tsz.y) * 0.5f)),
				tcol,
				items[i].label);

			if (!clicked)
				continue;

			switch (items[i].action)
			{
			case kProps:
				g_detail = g_ctx;
				g_detail.valid = true;
				g_detail_open = true;
				g_bc_for = 0;
				g_bc_bytes.clear();
				g_bc_status.clear();
				break;
			case kBytecode:
				g_detail = g_ctx;
				g_detail.valid = true;
				g_detail_open = true;
				DumpBytecode(g_detail);
				break;
			case kCopyName:
				ImGui::SetClipboardText(g_ctx.name.c_str());
				break;
			case kCopyClass:
				ImGui::SetClipboardText(g_ctx.cls.c_str());
				break;
			case kCopyPath:
				ImGui::SetClipboardText(g_ctx.path.c_str());
				break;
			case kCopyAddr:
			{
				char buf[32];
				std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)g_ctx.address);
				ImGui::SetClipboardText(buf);
				break;
			}
			}
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
}

void DrawDetailWindow(float window_alpha)
{
	if (!g_detail_open || !g_detail.valid)
		return;

	bool is_script = IsScriptClass(g_detail.cls);
	if (is_script)
		ImGui::SetNextWindowSize(ImVec2(300.f, 300.f), ImGuiCond_FirstUseEver);

	else
		ImGui::SetNextWindowSize(ImVec2(240.f, 150.f), ImGuiCond_FirstUseEver);

	ImGui::SetNextWindowSizeConstraints(ImVec2(200.f, 110.f), ImVec2(700.f, 800.f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
	PushWindowChrome();

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

	bool open = true;
	if (ImGui::Begin("##explorer_detail", &open, flags))
	{
		colors::draw_panel_background(window_alpha);

		float margin = 10.f;
		ImVec2 sz = ImGui::GetWindowSize();
		ImGui::SetCursorPos(ImVec2(margin, margin));

		if (widgets::begin_child_panel(
				"detail_child",
				ImVec2(sz.x - margin * 2.f, sz.y - margin * 2.f),
				"properties", fonts::ui_bold(), PanelFontSize(),
				nullptr, nullptr, nullptr))
		{
			const char* nm = "?";
			if (!g_detail.name.empty())
				nm = g_detail.name.c_str();

			ImGui::SetCursorPosX(6.f);
			TextLine(colors::accent_u32(), "%s", nm);

			ImGui::SetCursorPosX(6.f);
			TextLine(colors::text_active_u32(), "class: %s", g_detail.cls.c_str());
			ImGui::SetCursorPosX(6.f);
			ImGui::PushStyleColor(ImGuiCol_Text, colors::text_inactive);
			ImGui::TextWrapped("path: %s", g_detail.path.c_str());
			ImGui::PopStyleColor();
			ImGui::SetCursorPosX(6.f);
			TextLine(colors::text_active_u32(),
				"address: 0x%llX", (unsigned long long)g_detail.address);

			Cheat::Instance inst(g_detail.address);
			ImGui::SetCursorPosX(6.f);
			TextLine(colors::text_active_u32(), "children: %d", (int)inst.GetChildren().size());

			if (g_detail.cls == "Humanoid")
			{
				Cheat::Humanoid hum(g_detail.address);
				ImGui::SetCursorPosX(6.f);
				TextLine(colors::text_active_u32(),
					"health: %.1f / %.1f", hum.GetHealth(), hum.GetMaxHealth());
				ImGui::SetCursorPosX(6.f);
				TextLine(colors::text_active_u32(),
					"walkspeed: %.1f  jump: %.1f", hum.GetWalkSpeed(), hum.GetJumpPower());
			}

			if (IsScriptClass(g_detail.cls))
			{
				ImGui::Separator();
				ImGui::SetCursorPosX(6.f);
				if (widgets::button("dump"))
					DumpBytecode(g_detail);
				ImGui::SameLine();
				if (widgets::button("save"))
					SaveBytecodeToFile(g_detail);
				ImGui::SameLine();
				if (widgets::button("copy hex"))
				{
					std::string hex;
					hex.reserve(g_bc_bytes.size() * 2);
					static const char* d = "0123456789ABCDEF";
					for (unsigned char b : g_bc_bytes)
					{
						hex += d[b >> 4];
						hex += d[b & 0xF];
					}
					ImGui::SetClipboardText(hex.c_str());
				}

				ImGui::SetCursorPosX(6.f);
				const char* st = "bytecode is compressed; not source";
				if (!g_bc_status.empty())
					st = g_bc_status.c_str();
				TextLine(colors::text_inactive_u32(), "%s", st);

				ImGui::SetCursorPosX(6.f);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.25f));
				if (ImGui::BeginChild("##bc_hex", ImVec2(-6.f, -6.f), true,
						ImGuiWindowFlags_HorizontalScrollbar))
				{
					size_t max_show = g_bc_bytes.size();
					if (max_show > 4096)
						max_show = 4096;

					char line[80];
					for (size_t i = 0; i < max_show; i += 16)
					{
						std::string row;
						for (size_t j = 0; j < 16 && i + j < max_show; ++j)
						{
							std::snprintf(line, sizeof(line), "%02X ", g_bc_bytes[i + j]);
							row += line;
						}
						ImGui::TextUnformatted(row.c_str());
					}

					if (g_bc_bytes.size() > max_show)
						TextLine(colors::text_inactive_u32(), "... (%zu more bytes)",
							g_bc_bytes.size() - max_show);
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
		}
		widgets::end_child_panel();
	}
	ImGui::End();
	PopWindowChrome();
	ImGui::PopStyleVar();

	if (!open)
		g_detail_open = false;
}

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
