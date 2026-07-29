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

