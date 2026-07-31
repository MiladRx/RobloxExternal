// Fission in-process — без Server.exe. C++latest, без pch.
#include "FissionEmbed.h"

#include "Decompiler.hpp"

#include "Luau/Common.h"
#include "libassert/assert.hpp"

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Cheat {
namespace Features {
namespace FissionEmbed {
namespace {

void EnableLuauFFlags()
{
	static bool done = false;
	if (done)
		return;
	for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
	{
		if (flag->name && std::strncmp(flag->name, "Luau", 4) == 0)
			flag->value = true;
	}
	// assert → throw, не abort весь процесс
	libassert::set_failure_handler([](const libassert::assertion_info& info) -> void {
		throw std::runtime_error(info.to_string(0, libassert::color_scheme::blank));
	});
	done = true;
}

// fission срёт в cout "generated source code" — глушим
struct CoutMute
{
	std::ostringstream sink;
	std::streambuf* prev;
	CoutMute() : prev(std::cout.rdbuf(sink.rdbuf())) {}
	~CoutMute() { std::cout.rdbuf(prev); }
};

bool TryReadDecByte(const std::string& src, size_t at, int& val, size_t& end)
{
	if (at >= src.size() || src[at] < '0' || src[at] > '9')
		return false;
	val = 0;
	size_t j = at;
	int digits = 0;
	while (j < src.size() && digits < 3 && src[j] >= '0' && src[j] <= '9')
	{
		val = val * 10 + (src[j] - '0');
		++j;
		++digits;
	}
	if (digits == 0 || val > 255)
		return false;
	end = j;
	return true;
}

} // namespace

std::string DecompileLuauBytecode(const std::uint8_t* data, std::size_t n)
{
	if (!data || n == 0)
		return {};

	EnableLuauFFlags();

	const std::string bc(reinterpret_cast<const char*>(data), n);
	Decompiler dec{};
	const auto flags = DecompilerFlags::InferRobloxTypes | DecompilerFlags::InferTypes
		| DecompilerFlags::AutoNameVariables | DecompilerFlags::OmitFissionComments;

	DecompilationResult r{};
	try
	{
		CoutMute mute{};
		r = dec.DecompileRobloxBytecode(bc, flags);
	}
	catch (...)
	{
		return {};
	}
	if (r.resultCode != DecompileResult::Success)
		return {};

	std::string out = r.decompilationOutput;
	if (out.size() < 8)
		return {};

	try
	{
		UnescapeLuaByteEscapes(out);
	}
	catch (...)
	{
		return {};
	}
	return out;
}

void UnescapeLuaByteEscapes(std::string& src)
{
	std::string out;
	out.reserve(src.size());

	bool in_str = false;
	char quote = 0;

	for (size_t i = 0; i < src.size();)
	{
		const char c = src[i];

		if (!in_str)
		{
			if (c == '"' || c == '\'')
			{
				in_str = true;
				quote = c;
			}
			out.push_back(c);
			++i;
			continue;
		}

		if (c == quote)
		{
			in_str = false;
			quote = 0;
			out.push_back(c);
			++i;
			continue;
		}

		if (c != '\\' || i + 1 >= src.size())
		{
			out.push_back(c);
			++i;
			continue;
		}

		const char n1 = src[i + 1];

		// fission: deserializer пишет \208, generator ещё раз \\ → \\208
		if (n1 == '\\')
		{
			int val = 0;
			size_t end = 0;
			if (TryReadDecByte(src, i + 2, val, end))
			{
				out.push_back(static_cast<char>(val));
				i = end;
				continue;
			}
		}

		// \xHH
		if (n1 == 'x' || n1 == 'X')
		{
			if (i + 3 < src.size())
			{
				auto hex = [](char h) -> int {
					if (h >= '0' && h <= '9') return h - '0';
					if (h >= 'a' && h <= 'f') return h - 'a' + 10;
					if (h >= 'A' && h <= 'F') return h - 'A' + 10;
					return -1;
				};
				const int hi = hex(src[i + 2]);
				const int lo = hex(src[i + 3]);
				if (hi >= 0 && lo >= 0)
				{
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 4;
					continue;
				}
			}
		}

		// \ddd single
		if (n1 >= '0' && n1 <= '9')
		{
			int val = 0;
			size_t end = 0;
			if (TryReadDecByte(src, i + 1, val, end))
			{
				out.push_back(static_cast<char>(val));
				i = end;
				continue;
			}
		}

		// \n \t \\ \" — как есть
		out.push_back(c);
		out.push_back(n1);
		i += 2;
	}

	src.swap(out);
}

} // namespace FissionEmbed
} // namespace Features
} // namespace Cheat
