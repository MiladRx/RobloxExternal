#pragma once

#include "../State.h"

#include <Windows.h>
#include <cstring>
#include <string>

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline bool SetClipboardText(const std::string& text)
{
	if (!OpenClipboard(nullptr))
		return false;

	EmptyClipboard();
	const size_t n = text.size() + 1;
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
	if (!mem)
	{
		CloseClipboard();
		return false;
	}

	void* p = GlobalLock(mem);
	if (!p)
	{
		GlobalFree(mem);
		CloseClipboard();
		return false;
	}

	std::memcpy(p, text.c_str(), n);
	GlobalUnlock(mem);
	SetClipboardData(CF_TEXT, mem);
	CloseClipboard();
	return true;
}

inline std::string GetClipboardText()
{
	std::string out;
	if (!OpenClipboard(nullptr))
		return out;

	HANDLE h = GetClipboardData(CF_TEXT);
	if (h)
	{
		const char* p = static_cast<const char*>(GlobalLock(h));
		if (p)
		{
			out = p;
			GlobalUnlock(h);
		}
	}
	CloseClipboard();
	return out;
}

inline std::string BuildOutputText(bool selected_only)
{
	std::string out;
	if (selected_only && g_out_sel >= 0 && g_out_sel < (int)g_output.size())
	{
		out = g_output[g_out_sel].text;
		return out;
	}

	out.reserve(g_output.size() * 48);
	for (size_t i = 0; i < g_output.size(); ++i)
	{
		if (i)
			out.push_back('\n');
		out += g_output[i].text;
	}
	return out;
}

}
}
}
