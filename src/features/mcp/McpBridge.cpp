#include "pch.h"
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "McpBridge.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#undef GetClassName

namespace Cheat {
namespace Features {
namespace McpBridge {
namespace {

//constexpr unsigned short k_port = 3847;
//constexpr int k_max_children = 256;
//constexpr int k_max_search = 80;
//constexpr int k_max_depth = 8;

std::atomic<bool> g_run{ false };
std::thread g_thread;
SOCKET g_listen = INVALID_SOCKET;
std::mutex g_sock_mtx;

std::string JsonEscape(const std::string& s)
{
	std::string out;
	out.reserve(s.size() + 8);

	for (unsigned char c : s)
	{
		switch (c)
		{
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
				out += buf;
			}

			else
			{
				out += (char)c;
			}
			break;
		}
	}

	return out;
}

std::string QueryParam(const std::string& query, const char* key)
{
	std::string prefix = std::string(key) + "=";
	std::size_t pos = 0;

	while (pos < query.size())
	{
		std::size_t amp = query.find('&', pos);
		std::size_t len = std::string::npos;
		if (amp != std::string::npos)
			len = amp - pos;

		std::string part = query.substr(pos, len);

		if (part.rfind(prefix, 0) == 0)
		{
			std::string v = part.substr(prefix.size());
			// %20 / + и дальше лень нормально
			std::string out;
			for (std::size_t i = 0; i < v.size(); ++i)
			{
				if (v[i] == '+')
				{
					out.push_back(' ');
				}

				else if (v[i] == '%' && i + 2 < v.size())
				{
					auto hex = [](char c) -> int
					{
						if (c >= '0' && c <= '9') return c - '0';
						if (c >= 'a' && c <= 'f') return c - 'a' + 10;
						if (c >= 'A' && c <= 'F') return c - 'A' + 10;
						return -1;
					};

					int hi = hex(v[i + 1]);
					int lo = hex(v[i + 2]);
					if (hi >= 0 && lo >= 0)
					{
						out.push_back((char)((hi << 4) | lo));
						i += 2;
					}

					else
					{
						out.push_back(v[i]);
					}
				}

				else
				{
					out.push_back(v[i]);
				}
			}
			return out;
		}

		if (amp == std::string::npos)
			break;

		pos = amp + 1;
	}

	return {};
}

std::uint64_t ParseAddr(const std::string& s)
{
	if (s.empty())
		return 0;

	try
	{
		return (std::uint64_t)std::stoull(s, nullptr, 0);
	}
	catch (...)
	{
		return 0;
	}
}

Instance DataModelRoot()
{
	if (!g_Memory.IsAttached())
		return {};

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return {};

	return Instance(dm.address);
}

Instance ResolvePath(const Instance& root, const std::string& path)
{
	if (!root.address)
		return {};

	if (path.empty() || path == "/" || path == ".")
		return root;

	Instance cur = root;
	std::size_t i = 0;
	if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
		i = 1;

	while (i < path.size())
	{
		std::size_t j = path.find_first_of("/\\", i);
		if (j == std::string::npos)
			j = path.size();

		std::string part = path.substr(i, j - i);
		i = j + 1;

		if (part.empty() || part == ".")
			continue;

		if (part == "game" || part == "Game" || part == "DataModel")
			continue;

		auto child = cur.FindFirstChild(part);
		if (!child || !g_Memory.IsValid(child->address))
			return {};

		cur = *child;
	}

	return cur;
}

std::string InstanceJson(const Instance& inst)
{
	if (!g_Memory.IsValid(inst.address))
		return "{}";

	char addr[32];
	std::snprintf(addr, sizeof(addr), "0x%llX", (unsigned long long)inst.address);

	std::ostringstream o;
	o << "{\"name\":\"" << JsonEscape(inst.GetName())
	  << "\",\"class\":\"" << JsonEscape(inst.GetClassName())
	  << "\",\"address\":\"" << addr << "\"}";
	return o.str();
}

std::string HandleHealth()
{
	bool attached = g_Memory.IsAttached();
	const auto& dm = Globals::InstanceDataModel;

	std::uint64_t place = 0;
	std::uint64_t game = 0;
	if (attached && g_Memory.IsValid(dm.address))
	{
		place = dm.GetPlaceId();
		game = dm.GetGameId();
	}

	const char* att = "false";
	if (attached)
		att = "true";

	std::ostringstream o;
	o << "{\"ok\":true,\"attached\":" << att
	  << ",\"placeId\":" << place
	  << ",\"gameId\":" << game << "}";
	return o.str();
}

std::string HandleInfo()
{
	if (!g_Memory.IsAttached())
		return "{\"error\":\"not_attached\"}";

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return "{\"error\":\"no_datamodel\"}";

	const char* loaded = "false";
	if (dm.IsGameLoaded())
		loaded = "true";

	std::ostringstream o;
	o << "{\"placeId\":" << dm.GetPlaceId()
	  << ",\"gameId\":" << dm.GetGameId()
	  << ",\"placeVersion\":" << dm.GetPlaceVersion()
	  << ",\"creatorId\":" << dm.GetCreatorId()
	  << ",\"jobId\":\"" << JsonEscape(dm.GetJobId()) << "\""
	  << ",\"gameLoaded\":" << loaded
	  << ",\"dataModel\":\"0x" << std::hex
	  << (unsigned long long)dm.address << std::dec << "\"";

	if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
	{
		o << ",\"workspace\":\"0x" << std::hex
		  << (unsigned long long)Globals::Workspace->address
		  << std::dec << "\"";
	}

	o << "}";
	return o.str();
}

Instance ResolveTarget(const std::string& query)
{
	Instance root = DataModelRoot();
	if (!root.address)
		return {};

	std::string addr_s = QueryParam(query, "addr");
	if (!addr_s.empty())
	{
		std::uint64_t a = ParseAddr(addr_s);
		if (g_Memory.IsValid(a))
			return Instance(a);

		return {};
	}

	std::string path = QueryParam(query, "path");
	if (!path.empty())
		return ResolvePath(root, path);

	// по дефолту workspace, иначе dm
	if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
		return Instance(Globals::Workspace->address);

	return root;
}

std::string HandleChildren(const std::string& query)
{
	Instance target = ResolveTarget(query);
	if (!g_Memory.IsValid(target.address))
		return "{\"error\":\"not_found\"}";

	auto kids = target.GetChildren();
	int n = (int)kids.size();
	if (n > 256)
		n = 256;

	std::ostringstream o;
	o << "{\"parent\":" << InstanceJson(target) << ",\"count\":";
	o << kids.size() << ",\"children\":[";
	for (int i = 0; i < n; ++i)
	{
		if (i)
			o << ',';
		o << InstanceJson(kids[(std::size_t)i]);
	}

	if ((int)kids.size() > n)
		o << "],\"truncated\":true}";

	else
		o << "],\"truncated\":false}";

	return o.str();
}

void SearchRecurse(const Instance& node, const std::string& needle_lower,
                   std::vector<Instance>& out, int depth)
{
	// depth 8 / 80 hits дальше нет смысла
	if (depth > 8 || (int)out.size() >= 80)
		return;

	if (!g_Memory.IsValid(node.address))
		return;

	auto kids = node.GetChildren();
	for (const auto& c : kids)
	{
		if ((int)out.size() >= 80)
			return;

		if (!g_Memory.IsValid(c.address))
			continue;

		std::string name = c.GetName();
		std::string lower = name;
		for (char& ch : lower)
			ch = (char)std::tolower((unsigned char)ch);

		if (lower.find(needle_lower) != std::string::npos)
			out.push_back(c);

		SearchRecurse(c, needle_lower, out, depth + 1);
	}
}

std::string HandleSearch(const std::string& query)
{
	std::string name = QueryParam(query, "name");
	if (name.empty())
		return "{\"error\":\"missing_name\"}";

	int limit = 80;
	std::string lim = QueryParam(query, "limit");
	if (!lim.empty())
	{
		try
		{
			limit = std::stoi(lim);
		}
		catch (...) {}

		if (limit < 1) limit = 1;
		if (limit > 80) limit = 80;
	}

	Instance root = DataModelRoot();
	std::string path = QueryParam(query, "path");
	if (!path.empty())
	{
		root = ResolvePath(root, path);
	}

	else if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
	{
		root = Instance(Globals::Workspace->address);
	}

	if (!g_Memory.IsValid(root.address))
		return "{\"error\":\"no_root\"}";

	std::string needle = name;
	for (char& ch : needle)
		ch = (char)std::tolower((unsigned char)ch);

	std::vector<Instance> hits;
	hits.reserve((std::size_t)limit);
	SearchRecurse(root, needle, hits, 0);

	std::ostringstream o;
	o << "{\"query\":\"" << JsonEscape(name) << "\",\"count\":" << hits.size() << ",\"results\":[";

	int n = (int)hits.size();
	if (n > limit)
		n = limit;

	for (int i = 0; i < n; ++i)
	{
		if (i)
			o << ',';
		o << InstanceJson(hits[(std::size_t)i]);
	}

	o << "]}";
	return o.str();
}

std::string HandlePath(const std::string& query)
{
	Instance target = ResolveTarget(query);
	if (!g_Memory.IsValid(target.address))
		return "{\"error\":\"not_found\"}";

	// вверх по parent до dm
	std::vector<std::string> parts;
	Instance cur = target;
	for (int guard = 0; guard < 64 && g_Memory.IsValid(cur.address); ++guard)
	{
		parts.push_back(cur.GetName());
		auto parent = cur.GetParent();
		if (!parent || !g_Memory.IsValid(parent->address))
			break;

		if (parent->address == Globals::InstanceDataModel.address)
		{
			parts.push_back("DataModel");
			break;
		}

		cur = *parent;
	}

	std::string path;
	for (auto it = parts.rbegin(); it != parts.rend(); ++it)
	{
		if (!path.empty())
			path += '/';
		path += *it;
	}

	std::ostringstream o;
	o << "{\"instance\":" << InstanceJson(target)
	  << ",\"path\":\"" << JsonEscape(path) << "\"}";
	return o.str();
}

std::string Dispatch(const std::string& path, const std::string& query)
{
	if (!Cheat::g_Settings.misc.mcp)
		return "{\"error\":\"mcp_disabled\"}";

	if (path == "/health")
		return HandleHealth();

	if (path == "/info")
		return HandleInfo();

	if (path == "/children")
		return HandleChildren(query);

	if (path == "/search")
		return HandleSearch(query);

	if (path == "/path")
		return HandlePath(query);

	return "{\"error\":\"unknown_endpoint\"}";
}

void SendResponse(SOCKET client, int status, const char* status_text, const std::string& body)
{
	char header[256];
	int n = std::snprintf(header, sizeof(header),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n",
		status, status_text, body.size());

	if (n > 0)
		send(client, header, n, 0);

	if (!body.empty())
		send(client, body.data(), (int)body.size(), 0);
}

void HandleClient(SOCKET client)
{
	char buf[4096];
	int got = recv(client, buf, sizeof(buf) - 1, 0);
	if (got <= 0)
	{
		closesocket(client);
		return;
	}
	buf[got] = 0;

	// только GET /path?q HTTP/1.1, остальное пофиг
	std::string req(buf, buf + got);
	if (req.rfind("GET ", 0) != 0)
	{
		SendResponse(client, 405, "Method Not Allowed", "{\"error\":\"method\"}");
		closesocket(client);
		return;
	}

	std::size_t sp = req.find(' ', 4);
	if (sp == std::string::npos)
	{
		SendResponse(client, 400, "Bad Request", "{\"error\":\"bad_request\"}");
		closesocket(client);
		return;
	}

	std::string url = req.substr(4, sp - 4);
	std::string path = url;
	std::string query;
	std::size_t q = url.find('?');
	if (q != std::string::npos)
	{
		path = url.substr(0, q);
		query = url.substr(q + 1);
	}

	if (path.size() > 1 && path.back() == '/')
		path.pop_back();

	std::string body = Dispatch(path, query);
	int status = 200;
	const char* text = "OK";

	if (body.find("\"error\"") != std::string::npos)
	{
		if (body.find("mcp_disabled") != std::string::npos ||
			body.find("not_attached") != std::string::npos)
		{
			status = 503;
			text = "Service Unavailable";
		}

		else if (body.find("not_found") != std::string::npos ||
			body.find("unknown_endpoint") != std::string::npos)
		{
			status = 404;
			text = "Not Found";
		}

		else
		{
			status = 400;
			text = "Bad Request";
		}
	}

	SendResponse(client, status, text, body);
	closesocket(client);
}

void CloseListen()
{
	std::lock_guard<std::mutex> lk(g_sock_mtx);
	if (g_listen != INVALID_SOCKET)
	{
		closesocket(g_listen);
		g_listen = INVALID_SOCKET;
	}
}

void Loop()
{
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		g_run = false;
		return;
	}

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sock == INVALID_SOCKET)
	{
		WSACleanup();
		g_run = false;
		return;
	}

	BOOL yes = TRUE;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&yes, sizeof(yes));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(3847);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) != 0 ||
		listen(listen_sock, 8) != 0)
	{
		closesocket(listen_sock);
		WSACleanup();
		g_run = false;
		return;
	}

	{
		std::lock_guard<std::mutex> lk(g_sock_mtx);
		g_listen = listen_sock;
	}

	// select с таймаутом, иначе Stop висит
	while (g_run.load(std::memory_order_relaxed))
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(listen_sock, &fds);
		timeval tv{};
		tv.tv_sec = 0;
		tv.tv_usec = 250000;

		int sel = select(0, &fds, nullptr, nullptr, &tv);
		if (sel <= 0)
			continue;

		sockaddr_in peer{};
		int peer_len = sizeof(peer);
		SOCKET client = accept(listen_sock, (sockaddr*)&peer, &peer_len);
		if (client == INVALID_SOCKET)
			continue;

		char ip[INET_ADDRSTRLEN]{};
		inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
		if (std::string(ip) != "127.0.0.1")
		{
			closesocket(client);
			continue;
		}

		HandleClient(client);
	}

	CloseListen();
	WSACleanup();
}

}

void Start()
{
	if (g_run.exchange(true))
		return;

	g_thread = std::thread(Loop);
}

void Stop()
{
	if (!g_run.exchange(false))
		return;

	CloseListen();
	if (g_thread.joinable())
		g_thread.join();
}

bool Running()
{
	return g_run.load(std::memory_order_relaxed);
}

}
}
}
