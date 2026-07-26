#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#undef GetClassName

std::string Cheat::Instance::GetName() const
{
	if (!g_Memory.IsValid(this->address))
	{
		return "Unknown";
	}

	std::uint64_t name = g_Memory.Read<std::uint64_t>(this->address + Offsets::Instance::Name);

	if (g_Memory.IsValid(name))
	{
		return g_Memory.ReadString(name);
	}

	return "Unknown";
}

std::string Cheat::Instance::GetClassName() const
{
	if (!g_Memory.IsValid(this->address))
	{
		return "Unknown";
	}

	std::uint64_t desc = g_Memory.Read<std::uint64_t>(this->address + Offsets::Instance::ClassDescriptor);
	if (!g_Memory.IsValid(desc))
	{
		return "Unknown";
	}

	std::uint64_t cls = g_Memory.Read<std::uint64_t>(desc + Offsets::Instance::ClassName);
	if (g_Memory.IsValid(cls))
	{
		return g_Memory.ReadString(cls);
	}

	return "Unknown";
}

std::vector<Cheat::Instance> Cheat::Instance::GetChildren() const
{
	if (!address || !g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t kids = g_Memory.Read<std::uint64_t>(address + Offsets::Instance::ChildrenStart);
	if (!g_Memory.IsValid(kids))
	{
		return {};
	}

	std::uint64_t start = g_Memory.Read<std::uint64_t>(kids);
	std::uint64_t end = g_Memory.Read<std::uint64_t>(kids + Offsets::Instance::ChildrenEnd);

	if (!g_Memory.IsValid(start) || !g_Memory.IsValid(end) || start >= end)
	{
		return {};
	}

	std::vector<Cheat::Instance> out;
	out.reserve(32);

	// 10k потолок на всякий, а то улетим
	int count = 0;
	for (std::uint64_t ptr = start; ptr < end && count < 10000; ptr += 16, ++count)
	{
		std::uint64_t child = g_Memory.Read<std::uint64_t>(ptr);
		if (g_Memory.IsValid(child))
		{
			out.emplace_back(child);
		}
	}

	return out;
}

std::shared_ptr<Cheat::Instance> Cheat::Instance::FindFirstChild(std::string child) const
{
	for (auto& c : GetChildren())
	{
		if (c.GetName() == child)
		{
			return std::make_shared<Cheat::Instance>(c);
		}
	}

	return nullptr;
}

std::shared_ptr<Cheat::Instance> Cheat::Instance::GetParent() const
{
	if (!g_Memory.IsValid(address))
	{
		return nullptr;
	}

	std::uint64_t parent = g_Memory.Read<std::uint64_t>(address + Offsets::Instance::Parent);
	if (!g_Memory.IsValid(parent))
	{
		return nullptr;
	}

	return std::make_shared<Cheat::Instance>(parent);
}
