#pragma once

// icons / chrome / detail — тянется из Explorer.cpp в anonymous namespace

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
