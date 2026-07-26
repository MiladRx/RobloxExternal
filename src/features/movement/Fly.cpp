#include "pch.h"
#include "Fly.h"

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "renderer/Renderer.h"
#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>

#undef GetClassName

namespace {

using clock = std::chrono::steady_clock;

std::atomic<bool> g_fly_run{ false };
std::atomic<bool> g_grav_run{ false };
std::atomic<bool> g_fly_on{ false };
std::thread g_fly_th;
std::thread g_grav_th;

struct mat3
{
    float _11, _12, _13;
    float _21, _22, _23;
    float _31, _32, _33;
};

using NtWrite_t = LONG(__stdcall*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using NtRead_t  = LONG(__stdcall*)(HANDLE, PVOID, PVOID, ULONG, PULONG);

NtWrite_t g_nt_w = nullptr;
NtRead_t  g_nt_r = nullptr;

void grab_nt()
{
    static std::once_flag once;
    std::call_once(once, []() {
        HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            ntdll = ::LoadLibraryW(L"ntdll.dll");
        if (!ntdll)
            return;
        g_nt_w = reinterpret_cast<NtWrite_t>(::GetProcAddress(ntdll, "NtWriteVirtualMemory"));
        g_nt_r = reinterpret_cast<NtRead_t>(::GetProcAddress(ntdll, "NtReadVirtualMemory"));
    });
}

bool raw_write(std::uint64_t addr, const void* buf, std::size_t n)
{
	grab_nt();
	HANDLE proc = g_Memory.GetHandle();
	if (!proc || !addr || !buf || !n)
		return false;

	if (g_nt_w)
	{
		LONG st = g_nt_w(
			proc,
			(PVOID)addr,
			(void*)buf,
			(ULONG)n,
			nullptr);
		if (st >= 0)
			return true;
	}

	SIZE_T wrote = 0;
	return ::WriteProcessMemory(proc, (PVOID)addr, buf, n, &wrote) != 0 && wrote == n;
}

bool raw_read(std::uint64_t addr, void* buf, std::size_t n)
{
	grab_nt();
	HANDLE proc = g_Memory.GetHandle();
	if (!proc || !addr || !buf || !n)
		return false;

	if (g_nt_r)
	{
		LONG st = g_nt_r(
			proc,
			(PVOID)addr,
			buf,
			(ULONG)n,
			nullptr);
		if (st >= 0)
			return true;
	}

	SIZE_T got = 0;
	return ::ReadProcessMemory(proc, (LPCVOID)addr, buf, n, &got) != 0 && got == n;
}

template <typename T>
bool poke(std::uint64_t addr, const T& v)
{
    return raw_write(addr, &v, sizeof(T));
}

template <typename T>
T peek(std::uint64_t addr)
{
    T v{};
    raw_read(addr, &v, sizeof(T));
    return v;
}

enum class kb_layout
{
    qwerty,
    azerty
};

kb_layout detect_layout(HWND wnd)
{
	DWORD tid = 0;
	if (wnd)
		tid = GetWindowThreadProcessId(wnd, nullptr);

	HKL lay = GetKeyboardLayout(tid);
	LANGID lid = LOWORD((UINT_PTR)lay);
	if (PRIMARYLANGID(lid) == LANG_FRENCH)
		return kb_layout::azerty;

	return kb_layout::qwerty;
}

bool roblox_focused()
{
	HWND wnd = Cheat::Renderer::GetGameHwnd();
	if (!wnd || !::IsWindow(wnd))
		return false;

	return ::GetForegroundWindow() == wnd;
}

Vector3 cam_fwd(const mat3& r)
{
	Vector3 f(-r._13, -r._23, -r._33);
	float ls = f.LengthSquared();
	if (ls < 1e-4f || !std::isfinite(ls))
		return Vector3(0.0f, 0.0f, -1.0f);
	f.Normalize();
	return f;
}

Vector3 cam_right(const mat3& r)
{
	Vector3 rt(-r._11, r._21, -r._31);
	float ls = rt.LengthSquared();
	if (ls < 1e-4f || !std::isfinite(ls))
		return Vector3(1.0f, 0.0f, 0.0f);
	rt.Normalize();
	return rt;
}

bool key_gate(int key, int mode, bool& tog, bool& was)
{
	if (key == 0)
		return true;

	bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
	if (mode == 1)
	{
		if (down && !was)
			tog = !tog;
		was = down;
		return tog;
	}

	was = down;
	return down;
}

bool set_vel(std::uint64_t prim, const Vector3& vel)
{
	if (!prim)
		return false;

	poke(prim + Offsets::Primitive::AssemblyLinearVelocity, vel);
	Vector3 zero{};
	poke(prim + Offsets::Primitive::AssemblyAngularVelocity, zero);
	return true;
}

bool zero_vel(std::uint64_t prim)
{
    const Vector3 zero{};
    return set_vel(prim, zero);
}

struct fly_snap
{
    std::uint64_t address = 0;
    std::uint64_t hrp = 0;
    std::uint64_t prim = 0;
    std::uint64_t cam = 0;
};

fly_snap grab_local()
{
	fly_snap s{};

	if (!Cheat::Globals::Players || !Cheat::Globals::Players->address)
		return s;

	std::uint64_t lp = peek<std::uint64_t>(
		Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
	if (!lp)
		return s;

	std::uint64_t chara = peek<std::uint64_t>(
		lp + Offsets::Player::ModelInstance);
	if (!chara)
		return s;

	s.address = lp;

	auto hum = Cheat::Instance(chara).FindFirstChild("Humanoid");
	if (hum && hum->address)
	{
		s.hrp = Cheat::Humanoid(hum->address).GetRootPartAddress();
	}

	if (!s.hrp)
	{
		auto hrp = Cheat::Instance(chara).FindFirstChild("HumanoidRootPart");
		if (hrp)
			s.hrp = hrp->address;
	}

	if (s.hrp)
		s.prim = peek<std::uint64_t>(s.hrp + Offsets::BasePart::Primitive);

	if (Cheat::Globals::Workspace && Cheat::Globals::Workspace->address)
	{
		s.cam = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + Offsets::Workspace::CurrentCamera);
	}

	return s;
}

void touch_ptrs(fly_snap& s)
{
	if (s.hrp)
		s.prim = peek<std::uint64_t>(s.hrp + Offsets::BasePart::Primitive);

	if (Cheat::Globals::Workspace && Cheat::Globals::Workspace->address)
	{
		s.cam = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + Offsets::Workspace::CurrentCamera);
	}
}

// velocity луп, гравитация в другом треде
void fly_loop()
{
	bool was_on = false;
	Vector3 cur_vel{};

	bool tog = false;
	bool was_key = false;

	fly_snap cached{};
	auto last_res = clock::now() - std::chrono::seconds(1);

	while (g_fly_run.load(std::memory_order_relaxed))
	{
		auto now = clock::now();

		if (!Cheat::g_Settings.misc.fly)
		{
			tog = false;
			was_key = false;
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}
			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool focus = roblox_focused();
		bool can_tog = focus || Cheat::g_Settings.misc.fly_key == 0;

		bool key_on = false;
		if (can_tog)
		{
			key_on = key_gate(
				Cheat::g_Settings.misc.fly_key,
				Cheat::g_Settings.misc.fly_key_mode,
				tog,
				was_key);
		}

		if (!cached.hrp || !cached.cam)
		{
			if ((now - last_res) > std::chrono::milliseconds(50))
			{
				cached = grab_local();
				last_res = now;
			}
		}

		else if ((now - last_res) > std::chrono::milliseconds(250))
		{
			touch_ptrs(cached);
			last_res = now;
		}

		bool flying = Cheat::g_Settings.misc.fly && key_on;
		bool idle = !flying;
		g_fly_on.store(flying, std::memory_order_relaxed);

		if (cached.address == 0 || !cached.prim || !cached.cam)
		{
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}

			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;

			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		mat3 rot{};
		if (!raw_read(cached.cam + Offsets::Camera::Rotation, &rot, sizeof(rot)))
		{
			if (was_on && cached.prim)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}
			g_fly_on.store(false, std::memory_order_relaxed);
			was_on = false;
			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		Vector3 fwd = cam_fwd(rot);
		Vector3 right = cam_right(rot);
		Vector3 up(0.0f, 1.0f, 0.0f);

		if (!flying)
		{
			if (was_on)
			{
				zero_vel(cached.prim);
				cur_vel = {};
			}

			was_on = false;

			if (idle)
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		was_on = true;

		kb_layout lay = detect_layout(Cheat::Renderer::GetGameHwnd());
		int fwd_vk = lay == kb_layout::azerty ? 'Z' : 'W';
		int left_vk = lay == kb_layout::azerty ? 'Q' : 'A';

		auto held = [](int vk) -> bool
		{
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		};

		if (fwd.LengthSquared() < 1e-6f || right.LengthSquared() < 1e-6f)
		{
			fwd = Vector3(0.0f, 0.0f, 1.0f);
			right = Vector3(1.0f, 0.0f, 0.0f);
		}

		else
		{
			fwd.Normalize();
			right.Normalize();
		}

		Vector3 want{};
		float spd = Cheat::g_Settings.misc.fly_speed;
		if (spd < 0.f) spd = 0.f;

		if (focus || Cheat::g_Settings.misc.fly_key == 0)
		{
			if (held(fwd_vk)) want += fwd * spd;
			if (held('S'))    want -= fwd * spd;
			if (held(left_vk)) want += right * spd;
			if (held('D'))    want -= right * spd;

			float vert = 0.0f;
			if (held(VK_SPACE)) vert += 1.0f;
			if (held(VK_CONTROL)) vert -= 1.0f;
			if (std::abs(vert) > 0.0f)
				want += up * (vert * spd);
		}

		cur_vel = want;
		set_vel(cached.prim, cur_vel);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	{
		auto s = grab_local();
		if (s.address != 0 && s.prim)
			zero_vel(s.prim);
	}
	g_fly_on.store(false, std::memory_order_relaxed);
}

// гравитацию в ноль пока летаем
void grav_loop()
{
	bool overriden = false;
	float backup = 0.0f;

	while (g_grav_run.load(std::memory_order_relaxed))
	{
		if (!Cheat::g_Settings.misc.fly)
		{
			if (overriden &&
				Cheat::Globals::Workspace &&
				Cheat::Globals::Workspace->address)
			{
				auto world = peek<std::uint64_t>(
					Cheat::Globals::Workspace->address + Offsets::Workspace::World);
				if (world)
					poke(world + Offsets::World::Gravity, backup);
				overriden = false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool flying = g_fly_on.load(std::memory_order_relaxed);
		bool can =
			Cheat::Globals::Workspace &&
			Cheat::Globals::Workspace->address;

		if (flying && can)
		{
			auto world = peek<std::uint64_t>(
				Cheat::Globals::Workspace->address + Offsets::Workspace::World);
			if (world)
			{
				if (!overriden)
				{
					backup = peek<float>(world + Offsets::World::Gravity);
					overriden = true;
				}
				float z = 0.0f;
				poke(world + Offsets::World::Gravity, z);
			}
		}

		else if (!flying && overriden && can)
		{
			auto world = peek<std::uint64_t>(
				Cheat::Globals::Workspace->address + Offsets::Workspace::World);
			if (world)
				poke(world + Offsets::World::Gravity, backup);
			overriden = false;
		}

		if (flying)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		else
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}

	if (overriden &&
		Cheat::Globals::Workspace &&
		Cheat::Globals::Workspace->address)
	{
		auto world = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + Offsets::Workspace::World);
		if (world)
			poke(world + Offsets::World::Gravity, backup);
	}
}

}

void Cheat::Features::Fly::Start()
{
	if (!g_fly_run.load(std::memory_order_relaxed))
	{
		g_fly_run = true;
		g_fly_th = std::thread(fly_loop);
	}

	if (!g_grav_run.load(std::memory_order_relaxed))
	{
		g_grav_run = true;
		g_grav_th = std::thread(grav_loop);
	}
}

void Cheat::Features::Fly::Stop()
{
	g_fly_run.store(false, std::memory_order_relaxed);
	g_grav_run.store(false, std::memory_order_relaxed);
	if (g_fly_th.joinable())
		g_fly_th.join();
	if (g_grav_th.joinable())
		g_grav_th.join();
}
