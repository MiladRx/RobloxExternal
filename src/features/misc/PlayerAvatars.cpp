#include "pch.h"
#include "PlayerAvatars.h"

#include "core/player/PlayerHandler.h"
#include "app/Graphics.h"

#include <stb_image.h>
#include <imgui.h>
#include <Windows.h>
#include <winhttp.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

namespace Cheat {
namespace Features {
namespace PlayerAvatars {
namespace {

	struct Tex {
		ID3D11ShaderResourceView* srv = nullptr;
	};

	struct Pending {
		std::int64_t uid = 0;
		std::vector<std::uint8_t> png;
	};

	std::mutex g_mu;
	std::unordered_map<std::int64_t, Tex> g_tex;
	std::unordered_map<std::int64_t, float> g_fail_at; // ImGui time, retry
	std::unordered_set<std::int64_t> g_queued;
	std::unordered_set<std::int64_t> g_loading;
	std::vector<std::int64_t> g_queue;
	std::vector<Pending> g_ready;

	std::atomic<int> g_alive{ 0 };
	const int g_max_workers = 12;

	bool HttpGet(const wchar_t* host, INTERNET_PORT port, const wchar_t* path,
	             bool https, std::vector<std::uint8_t>& out)
	{
		out.clear();
		HINTERNET ses = WinHttpOpen(
			L"jewsploit/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0);
		if (!ses)
		{
			return false;
		}

		HINTERNET con = WinHttpConnect(ses, host, port, 0);
		if (!con)
		{
			WinHttpCloseHandle(ses);
			return false;
		}

		DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(
			con, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			WinHttpCloseHandle(con);
			WinHttpCloseHandle(ses);
			return false;
		}

		DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

		BOOL ok = WinHttpSendRequest(
			req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (!ok || !WinHttpReceiveResponse(req, nullptr))
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(con);
			WinHttpCloseHandle(ses);
			return false;
		}

		for (;;)
		{
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
			{
				break;
			}

			std::size_t old = out.size();
			out.resize(old + avail);
			DWORD read = 0;
			if (!WinHttpReadData(req, out.data() + old, avail, &read))
			{
				out.resize(old);
				break;
			}

			out.resize(old + read);
			if (out.size() > (4u << 20))
			{
				break;
			}
		}

		WinHttpCloseHandle(req);
		WinHttpCloseHandle(con);
		WinHttpCloseHandle(ses);
		return !out.empty();
	}

	bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, bool& https)
	{
		https = true;
		const char* p = url.c_str();
		if (std::strncmp(p, "https://", 8) == 0)
		{
			p += 8;
			https = true;
		}

		else if (std::strncmp(p, "http://", 7) == 0)
		{
			p += 7;
			https = false;
		}

		else
		{
			return false;
		}

		const char* slash = std::strchr(p, '/');
		std::string h;
		std::string path_a;
		if (!slash)
		{
			h = p;
			path_a = "/";
		}

		else
		{
			h.assign(p, slash);
			path_a = slash;
		}

		if (h.empty())
		{
			return false;
		}

		host.assign(h.begin(), h.end());
		path.assign(path_a.begin(), path_a.end());
		return true;
	}

	std::string FindImageUrl(const std::string& json, std::int64_t uid)
	{
		char key[64]{};
		std::snprintf(key, sizeof(key), "\"targetId\":%lld", (long long)uid);

		std::size_t pos = json.find(key);
		if (pos == std::string::npos)
		{
			// пробелы в json иногда
			std::snprintf(key, sizeof(key), "\"targetId\": %lld", (long long)uid);
			pos = json.find(key);
		}

		std::size_t from = (pos == std::string::npos) ? 0 : pos;
		std::size_t u = json.find("\"imageUrl\"", from);
		if (u == std::string::npos)
		{
			return {};
		}

		u = json.find(':', u);
		if (u == std::string::npos)
		{
			return {};
		}

		u = json.find('"', u);
		if (u == std::string::npos)
		{
			return {};
		}

		std::size_t a = u + 1;
		std::size_t b = json.find('"', a);
		if (b == std::string::npos || b <= a)
		{
			return {};
		}

		std::string url = json.substr(a, b - a);
		// \/ в json
		for (std::size_t i = 0; i + 1 < url.size();)
		{
			if (url[i] == '\\' && url[i + 1] == '/')
			{
				url.erase(i, 1);
			}

			else
			{
				++i;
			}
		}

		return url;
	}

	bool FetchPng(std::int64_t uid, std::vector<std::uint8_t>& png)
	{
		png.clear();
		if (uid <= 0)
		{
			return false;
		}

		wchar_t path[160]{};
		_snwprintf_s(
			path, _TRUNCATE,
			L"/v1/users/avatar-bust?userIds=%lld&size=420x420&format=Png&isCircular=false",
			(long long)uid);

		std::vector<std::uint8_t> raw;
		if (!HttpGet(L"thumbnails.roblox.com", INTERNET_DEFAULT_HTTPS_PORT, path, true, raw))
		{
			return false;
		}

		std::string json(raw.begin(), raw.end());
		if (json.find("\"Pending\"") != std::string::npos &&
			json.find("\"imageUrl\"") == std::string::npos)
		{
			return false;
		}

		std::string img = FindImageUrl(json, uid);
		if (img.empty())
		{
			return false;
		}

		std::wstring host, pth;
		bool https = true;
		if (!ParseUrl(img, host, pth, https))
		{
			return false;
		}

		INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
		return HttpGet(host.c_str(), port, pth.c_str(), https, png);
	}

	void WorkerLoop()
	{
		for (;;)
		{
			std::int64_t uid = 0;
			{
				std::lock_guard<std::mutex> lk(g_mu);
				if (g_queue.empty())
				{
					break;
				}

				uid = g_queue.back();
				g_queue.pop_back();
				g_queued.erase(uid);
				g_loading.insert(uid);
			}

			std::vector<std::uint8_t> png;
			bool ok = FetchPng(uid, png);

			{
				std::lock_guard<std::mutex> lk(g_mu);
				g_loading.erase(uid);
				if (ok && !png.empty())
				{
					Pending p{};
					p.uid = uid;
					p.png = std::move(png);
					g_ready.push_back(std::move(p));
				}

				else
				{
					// ретрай позже
					g_fail_at[uid] = -1.f;
				}
			}
		}

		g_alive.fetch_sub(1);
	}

	// жрём очередь пачкой тредов
	void KickWorkers()
	{
		for (;;)
		{
			int cur = g_alive.load();
			if (cur >= g_max_workers)
			{
				return;
			}

			{
				std::lock_guard<std::mutex> lk(g_mu);
				if (g_queue.empty())
				{
					return;
				}
			}

			if (!g_alive.compare_exchange_weak(cur, cur + 1))
			{
				continue;
			}

			std::thread(WorkerLoop).detach();
		}
	}

	ID3D11ShaderResourceView* MakeSrv(const std::vector<std::uint8_t>& png)
	{
		if (!Core::g_Device || png.empty())
		{
			return nullptr;
		}

		int w = 0, h = 0, n = 0;
		unsigned char* px = stbi_load_from_memory(
			png.data(), (int)png.size(), &w, &h, &n, 4);
		if (!px)
		{
			return nullptr;
		}

		D3D11_TEXTURE2D_DESC td{};
		td.Width = (UINT)w;
		td.Height = (UINT)h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = px;
		sd.SysMemPitch = (UINT)(w * 4);

		ID3D11Texture2D* tex = nullptr;
		HRESULT hr = Core::g_Device->CreateTexture2D(&td, &sd, &tex);
		stbi_image_free(px);
		if (FAILED(hr) || !tex)
		{
			return nullptr;
		}

		ID3D11ShaderResourceView* srv = nullptr;
		hr = Core::g_Device->CreateShaderResourceView(tex, nullptr, &srv);
		tex->Release();
		if (FAILED(hr))
		{
			return nullptr;
		}

		return srv;
	}

	void UploadReady()
	{
		std::vector<Pending> ready;
		{
			std::lock_guard<std::mutex> lk(g_mu);
			ready.swap(g_ready);
		}

		for (auto& p : ready)
		{
			ID3D11ShaderResourceView* srv = MakeSrv(p.png);
			if (!srv)
			{
				std::lock_guard<std::mutex> lk(g_mu);
				g_fail_at[p.uid] = -1.f;
				continue;
			}

			std::lock_guard<std::mutex> lk(g_mu);
			auto it = g_tex.find(p.uid);
			if (it != g_tex.end() && it->second.srv)
			{
				it->second.srv->Release();
			}

			g_tex[p.uid].srv = srv;
			g_fail_at.erase(p.uid);
		}
	}

	void EnqueueWanted()
	{
		std::vector<std::int64_t> want;
		PlayerHandler::ForEachPlayer([&](const PlayerCache& c)
		{
			if (!c.is_player || c.user_id <= 0)
			{
				return;
			}

			want.push_back(c.user_id);
		});

		float now = (float)ImGui::GetTime();
		bool kick = false;
		{
			std::lock_guard<std::mutex> lk(g_mu);

			for (std::int64_t uid : want)
			{
				if (g_tex.count(uid))
				{
					continue;
				}

				if (g_queued.count(uid) || g_loading.count(uid))
				{
					continue;
				}

				auto fit = g_fail_at.find(uid);
				if (fit != g_fail_at.end())
				{
					float t = fit->second;
					if (t < 0.f)
					{
						fit->second = now + 2.5f;
						continue;
					}

					if (now < t)
					{
						continue;
					}

					g_fail_at.erase(fit);
				}

				g_queue.push_back(uid);
				g_queued.insert(uid);
				kick = true;
			}

			// кап очереди
			while (g_queue.size() > 96)
			{
				std::int64_t drop = g_queue.front();
				g_queue.erase(g_queue.begin());
				g_queued.erase(drop);
			}
		}

		if (!kick)
		{
			std::lock_guard<std::mutex> lk(g_mu);
			if (!g_queue.empty() && g_alive.load() < g_max_workers)
			{
				kick = true;
			}
		}

		if (kick)
		{
			KickWorkers();
		}
	}

} // namespace

void Tick()
{
	UploadReady();
	EnqueueWanted();
}

void Clear()
{
	{
		std::lock_guard<std::mutex> lk(g_mu);
		g_queue.clear();
		g_queued.clear();
		g_loading.clear();
		g_ready.clear();
		g_fail_at.clear();
		for (auto& kv : g_tex)
		{
			if (kv.second.srv)
			{
				kv.second.srv->Release();
			}
		}
		g_tex.clear();
	}
}

ID3D11ShaderResourceView* Get(std::int64_t user_id)
{
	if (user_id <= 0)
	{
		return nullptr;
	}

	std::lock_guard<std::mutex> lk(g_mu);
	auto it = g_tex.find(user_id);
	if (it == g_tex.end())
	{
		return nullptr;
	}

	return it->second.srv;
}

}
}
}
