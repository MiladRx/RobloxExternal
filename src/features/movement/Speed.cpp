#include "pch.h"
#include "Speed.h"

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
#include <thread>

#undef GetClassName

namespace {

std::atomic<bool> g_run{ false };
std::thread g_th;

enum class ws_mode
{
	position = 0,
	humanoid
};

bool chat_focused()
{
	if (!Cheat::Globals::InstanceDataModel.address)
		return false;

	static std::uint64_t s_bar = 0;
	static std::uint64_t s_dm = 0;

	if (s_dm != Cheat::Globals::InstanceDataModel.address || !g_Memory.IsValid(s_bar))
	{
		s_dm = Cheat::Globals::InstanceDataModel.address;
		s_bar = 0;
		for (const auto& c : Cheat::Globals::InstanceDataModel.GetChildren())
		{
			if (c.GetName() != "TextChatService" && c.GetClassName() != "TextChatService")
				continue;

			for (const auto& cfg : c.GetChildren())
			{
				if (cfg.GetName() == "ChatInputBarConfiguration")
				{
					s_bar = cfg.address;
					break;
				}
			}
			break;
		}
	}

	if (!g_Memory.IsValid(s_bar))
		return false;

	std::uint32_t flags = g_Memory.Read<std::uint32_t>(s_bar + Offsets::Chat::IsFocused);
	return (flags & 0x10000u) != 0;
}

bool roblox_focused()
{
    const HWND wnd = Cheat::Renderer::GetGameHwnd();
    return wnd && IsWindow(wnd) && GetForegroundWindow() == wnd;
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

std::uint64_t local_chara()
{
	if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
		return 0;

	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
	if (!g_Memory.IsValid(lp))
		return 0;

	std::uint64_t chara = g_Memory.Read<std::uint64_t>(
		lp + Offsets::Player::ModelInstance);
	if (!g_Memory.IsValid(chara))
		return 0;

	return chara;
}

bool resolve_local(std::uint64_t& out_hrp, std::uint64_t& out_prim, std::uint64_t& out_hum)
{
	out_hrp = 0;
	out_prim = 0;
	out_hum = 0;

	std::uint64_t chara = local_chara();
	if (!chara)
		return false;

	for (const auto& c : Cheat::Instance(chara).GetChildren())
	{
		if (!out_hum && c.GetClassName() == "Humanoid")
			out_hum = c.address;

		if (!out_hrp && c.GetName() == "HumanoidRootPart")
		{
			out_hrp = c.address;
			out_prim = g_Memory.Read<std::uint64_t>(out_hrp + Offsets::BasePart::Primitive);
		}
	}

	return g_Memory.IsValid(out_hrp) && g_Memory.IsValid(out_prim) && g_Memory.IsValid(out_hum);
}

float read_ws(std::uint64_t hum)
{
	if (!g_Memory.IsValid(hum))
		return 16.f;

	float v = g_Memory.Read<float>(hum + Offsets::Humanoid::Walkspeed);
	if (!std::isfinite(v))
		return 16.f;

	return v;
}

bool write_ws(std::uint64_t hum, float spd)
{
	if (!g_Memory.IsValid(hum) || !std::isfinite(spd))
		return false;

	g_Memory.Write<float>(hum + Offsets::Humanoid::Walkspeed, spd);
	g_Memory.Write<float>(hum + Offsets::Humanoid::WalkspeedCheck, spd);
	return true;
}

bool move_vel(std::uint64_t hum, float spd, Vector3& out)
{
	out = {};
	if (!g_Memory.IsValid(hum) || spd <= 0.0f)
		return false;

	Vector3 md = Cheat::Humanoid(hum).GetMoveDirection();
	Vector3 dir(md.x, 0.0f, md.z);
	float ls = dir.LengthSquared();
	if (!std::isfinite(ls) || ls < 1e-4f)
		return false;

	float len = std::sqrt(ls);
	if (!std::isfinite(len) || len < 1e-4f)
		return false;

	if (len > 1.0f)
		dir /= len;

	out = dir * spd;
	return true;
}

bool write_xz(std::uint64_t addr, float x, float z)
{
    if (!addr)
        return false;
    g_Memory.Write<float>(addr, x);
    g_Memory.Write<float>(addr + sizeof(float) * 2, z);
    return true;
}

bool set_xz_pos(std::uint64_t prim, float x, float z)
{
	if (!prim)
		return false;

	bool ok = false;
	ok |= write_xz(prim + Offsets::Primitive::Position, x, z);

	auto ptr_ok = [](std::uint64_t p) -> bool
	{
		// хз без этого иногда мусор ловит
		return p >= 0x10000 && p <= 0x00007FFFFFFFFFFF;
	};

	std::uint64_t props = g_Memory.Read<std::uint64_t>(
		prim + Offsets::Primitive::Properties);
	if (ptr_ok(props))
	{
		ok |= write_xz(props + Offsets::Primitive::PropertyPosition, x, z);
	}

	else
	{
		// иногда Properties не указатель а инлайн
		ok |= write_xz(
			prim + Offsets::Primitive::Properties + Offsets::Primitive::PropertyPosition,
			x,
			z);
	}

	return ok;
}

bool step_xz(std::uint64_t prim, const Vector3& hvel, float dt, int reps)
{
	if (dt <= 0.0f || !g_Memory.IsValid(prim))
		return false;

	if (reps < 1) reps = 1;
	if (reps > 100) reps = 100;

	float step = dt / (float)reps;
	bool ok = false;
	for (int i = 0; i < reps; ++i)
	{
		Vector3 pos = g_Memory.Read<Vector3>(prim + Offsets::Primitive::Position);
		float nx = pos.x + hvel.x * step;
		float nz = pos.z + hvel.z * step;
		ok |= set_xz_pos(prim, nx, nz);
	}
	return ok;
}

// спид крч либо ws либо позицию дёргаем
void speed_loop()
{
	bool was_on = false;
	bool got_bak = false;
	std::uint64_t bak_hum = 0;
	float bak_ws = 16.f;
	bool tog = false;
	bool was_key = false;
	auto last = std::chrono::steady_clock::now();

	auto clear_bak = [&]()
	{
		got_bak = false;
		bak_hum = 0;
		bak_ws = 16.f;
	};

	auto restore_ws = [&]()
	{
		if (got_bak && g_Memory.IsValid(bak_hum))
			write_ws(bak_hum, bak_ws);
		clear_bak();
	};

	auto backup_ws = [&](std::uint64_t hum)
	{
		if (!g_Memory.IsValid(hum))
			return;

		bool changed = !got_bak || bak_hum != hum;
		if (!changed)
			return;

		restore_ws();
		bak_ws = read_ws(hum);
		bak_hum = hum;
		got_bak = true;
	};

	while (g_run.load())
	{
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - last).count();
		if (dt < 0.0001f)
			dt = 0.0001f;
		last = now;

		bool in_chat = chat_focused();
		bool can_tog = roblox_focused() && !in_chat;

		if (!Cheat::g_Settings.misc.walkspeed)
		{
			tog = false;
			was_key = false;
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		bool key_on = false;
		if (can_tog || Cheat::g_Settings.misc.walkspeed_key == 0)
		{
			key_on = key_gate(
				Cheat::g_Settings.misc.walkspeed_key,
				Cheat::g_Settings.misc.walkspeed_key_mode,
				tog,
				was_key);
		}

		else if (in_chat && Cheat::g_Settings.misc.walkspeed_key)
		{
			GetAsyncKeyState(Cheat::g_Settings.misc.walkspeed_key);
			key_on = false;
		}

		float ws_val = Cheat::g_Settings.misc.walkspeed_value;
		if (ws_val < 1.f) ws_val = 1.f;
		if (ws_val > 500.f) ws_val = 500.f;
		Cheat::g_Settings.misc.walkspeed_value = ws_val;

		int ws_mode_i = Cheat::g_Settings.misc.walkspeed_mode;
		if (ws_mode_i < 0) ws_mode_i = 0;
		if (ws_mode_i > 1) ws_mode_i = 1;
		Cheat::g_Settings.misc.walkspeed_mode = ws_mode_i;

		if (!key_on)
		{
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		std::uint64_t hrp = 0, prim = 0, hum = 0;
		bool ok = resolve_local(hrp, prim, hum);
		auto mode = (ws_mode)Cheat::g_Settings.misc.walkspeed_mode;
		float spd = Cheat::g_Settings.misc.walkspeed_value;

		if (!ok)
		{
			if (was_on)
				restore_ws();
			was_on = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
			continue;
		}

		backup_ws(hum);

		// 0 = pos spam, 1 = чистый walkspeed
		if (mode == ws_mode::humanoid)
		{
			write_ws(hum, spd);
		}

		else
		{
			write_ws(hum, 0.1f);

			Vector3 want{};
			if (move_vel(hum, spd, want))
			{
				float cdt = dt;
				if (cdt < 0.001f) cdt = 0.001f;
				if (cdt > 0.05f) cdt = 0.05f;
				step_xz(prim, want, cdt, 16);
			}
		}

		was_on = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	restore_ws();
}

}

void Cheat::Features::Speed::Start()
{
	if (g_run.load())
		return;

	g_run.store(true);
	g_th = std::thread(speed_loop);
}

void Cheat::Features::Speed::Stop()
{
	g_run.store(false);
	if (g_th.joinable())
		g_th.join();
}
