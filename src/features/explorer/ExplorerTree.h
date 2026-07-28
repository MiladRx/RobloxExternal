#pragma once

// дерево нод — после ExplorerDraw.h (IconSrv / TextLine / g_ctx)

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
