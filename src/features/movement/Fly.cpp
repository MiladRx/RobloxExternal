#include "pch.h"
#include "Fly.h"
#include "FlyHelpers.h"

#include "app/Settings.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>

#undef GetClassName

namespace {

using clock = std::chrono::steady_clock;
using namespace Cheat::Features::Fly::helpers;

std::atomic<bool> g_fly_run{ false };
std::atomic<bool> g_grav_run{ false };
std::atomic<bool> g_fly_on{ false };
std::thread g_fly_th;
std::thread g_grav_th;

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
