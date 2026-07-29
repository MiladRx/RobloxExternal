#include "misc.h"
#include "helpers.h"
#include "../widgets/child.h"
#include "../widgets/checkbox.h"
#include "../widgets/keybind.h"
#include "../widgets/label_color.h"
#include "app/Settings.h"
#include "features/visuals/KillEffects.h"
#include "features/misc/HitSounds.h"

#include <imgui.h>

void ng_tabs::draw_misc_tab()
{
	using namespace Cheat;

	float side_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - side_gap) * 0.5f;

	Settings::AimbotConfig& cfg = g_Settings.aim.active();

	ng::child_begin("##misc_child1", "world", cw, avail_h, 10.f, true);

	pad();
	ng::checkbox("teamcheck", &Cheat::g_Settings.misc.teamcheck);
	gap();
	pad();
	ng::checkbox("no shadow", &Cheat::g_Settings.world.no_shadow);
	gap();
	row_slider("brightness", "brightness", &Cheat::g_Settings.world.brightness, 0.f, 20.f, true);
	gap();
	pad();
	ng::checkbox("time changer", &Cheat::g_Settings.world.time_changer);
	row_slider("clock time", "clock time", &Cheat::g_Settings.world.clock_time, 0.f, 24.f, Cheat::g_Settings.world.time_changer);
	gap();
	row_cb_color("fog", &Cheat::g_Settings.world.fog, Cheat::g_Settings.world.fog_color, "world_fog_color");
	gap();
	row_slider("fog start", "fog start", &Cheat::g_Settings.world.fog_start, 0.f, 100.f, true);
	gap();
	row_slider("fog end", "fog end", &Cheat::g_Settings.world.fog_end, 0.f, 2000.f, true);
	gap();
	pad();
	ng::checkbox("fps unlocker", &Cheat::g_Settings.misc.fps_unlock);
	gap();
	row_slider_i("fps cap", "fps cap", &Cheat::g_Settings.misc.fps_cap, 60, 1000, true);
	gap();
	pad();
	ng::checkbox("fov changer", &Cheat::g_Settings.misc.fov);
	gap();
	row_slider("fov value", "fov value", &Cheat::g_Settings.misc.fov_value, 10.f, 120.f, true);
	gap();
	pad();
	ng::checkbox("crosshair", &Cheat::g_Settings.crosshair.enabled);

	if (Cheat::g_Settings.crosshair.enabled)
	{
		gap();
		row_slider("cross length", "cross length", &Cheat::g_Settings.crosshair.length, 1.f, 40.f, true);
		gap();
		row_slider("cross gap", "cross gap", &Cheat::g_Settings.crosshair.gap, 0.f, 30.f, true);
		gap();
		row_slider("cross thick", "cross thick", &Cheat::g_Settings.crosshair.thickness, 1.f, 6.f, true);
		gap();
		pad();
		ng::checkbox("cross spin", &Cheat::g_Settings.crosshair.spin);
		row_slider("spin speed", "spin speed", &Cheat::g_Settings.crosshair.spin_speed, -360.f, 360.f, Cheat::g_Settings.crosshair.spin);
		gap();
		pad();
		ng::checkbox("cross outline", &Cheat::g_Settings.crosshair.outline);
		gap();
		pad();
		ng::checkbox("cross dot", &Cheat::g_Settings.crosshair.dot);
		row_slider("dot size", "dot size", &Cheat::g_Settings.crosshair.dot_size, 1.f, 8.f, Cheat::g_Settings.crosshair.dot);
		gap();
		pad();
		ng::label_color("crosshair_color", "cross color", Cheat::g_Settings.crosshair.color);
		gap();
		pad();
		ng::label_color("crosshair_outline_color", "outline color", Cheat::g_Settings.crosshair.outline_color);
	}

	ng::child_end();

	ImGui::SameLine(0.f, side_gap);

	ng::child_begin("##misc_child2", "local", cw, avail_h, 10.f, true);

	static const char* k_ws_modes[] = { "position", "humanoid" };

	pad();
	ng::checkbox("walkspeed", &Cheat::g_Settings.misc.walkspeed);
	gap();
	row_keybind("ws key", "ws key", &Cheat::g_Settings.misc.walkspeed_key, &Cheat::g_Settings.misc.walkspeed_key_mode);
	gap();
	row_select("ws mode", "ws mode", &Cheat::g_Settings.misc.walkspeed_mode, k_ws_modes, 2);
	gap();
	row_slider("ws value", "ws value", &Cheat::g_Settings.misc.walkspeed_value, 1.f, 500.f, true);
	gap();
	pad();
	ng::checkbox("jump power", &Cheat::g_Settings.misc.jump);
	gap();
	row_slider("jump value", "jump value", &Cheat::g_Settings.misc.jump_power, 50.f, 500.f, true);
	gap();
	pad();
	ng::checkbox("fly", &Cheat::g_Settings.misc.fly);
	gap();
	row_keybind("fly key", "fly key", &Cheat::g_Settings.misc.fly_key, &Cheat::g_Settings.misc.fly_key_mode);
	gap();
	row_slider("fly speed", "fly speed", &Cheat::g_Settings.misc.fly_speed, 5.f, 1000.f, true);
	gap();
	row_keybind("freecam", "freecam", &Cheat::g_Settings.misc.freecam_key, &Cheat::g_Settings.misc.freecam_mode);
	gap();
	row_slider("freecam speed", "freecam speed", &Cheat::g_Settings.misc.freecam_speed, 10.f, 300.f, true);
	gap();
	row_slider("freecam sens", "freecam sens", &Cheat::g_Settings.misc.freecam_sens, 0.05f, 1.f, true);
	gap();
	pad();
	ng::checkbox("third person", &Cheat::g_Settings.misc.third_person);
	ng::keybind("##third_person_kb", &Cheat::g_Settings.misc.third_person_key, &Cheat::g_Settings.misc.third_person_mode, Cheat::g_Settings.misc.third_person);
	gap();
	row_slider("camera back/up", "camera back/up", &Cheat::g_Settings.misc.third_person_distance, 1.f, 120.f, true);
	gap();
	pad();
	ng::checkbox("noclip", &Cheat::g_Settings.misc.noclip);
	gap();
	pad();
	ng::checkbox("infinite jump", &Cheat::g_Settings.misc.inf_jump);
	gap();

	pad();
	ng::checkbox("hitchance", &cfg.hitchance_enabled);
	gap();

	if (cfg.hitchance_enabled)
	{
		row_slider("##hitchance", "hit chance", &cfg.hitchance, 1.0f, 100.0f);
		gap();
	}

	pad();
	ng::checkbox("hitbox expander", &g_Settings.hitbox.enabled);
	gap();

	if (g_Settings.hitbox.enabled)
	{
		static const char* hb_parts[] = {
			"head", "hrp", "torso", "arms", "legs"
		};
		row_select("##hb_part", "hb part", &g_Settings.hitbox.part, hb_parts,
		           Settings::HB_PART_COUNT);
		gap();

		row_slider("##hb_scale", "hb scale", &g_Settings.hitbox.scale, 1.0f, 20.0f);
		gap();

		pad();
		ng::checkbox("hb visualize", &g_Settings.hitbox.visualize);
		gap();

		if (g_Settings.hitbox.visualize)
		{
			static const char* hb_viz[] = { "2d", "3d", "3d filled" };
			row_select("##hb_viz", "hb viz mode", &g_Settings.hitbox.viz_mode, hb_viz, 3);
			gap();

			ImGui::SetCursorPosX(12.f);
			ng::label_color("hitbox_viz_color", "hb color", g_Settings.hitbox.viz_color);
			gap();
		}
	}

	pad();
	ng::checkbox("kill effects", &g_Settings.killfx.enabled);
	gap();

	if (g_Settings.killfx.enabled)
	{
		if (g_Settings.killfx.effect < 0 ||
		    g_Settings.killfx.effect >= Visuals::KillEffects::EffectNameCount())
			g_Settings.killfx.effect = 0;

		row_select("##kill_fx", "kill fx", &g_Settings.killfx.effect,
		           Visuals::KillEffects::EffectNames(),
		           Visuals::KillEffects::EffectNameCount());
		gap();
	}

	pad();
	ng::checkbox("hitmarkers", &g_Settings.hitmarker.enabled);
	gap();

	if (g_Settings.hitmarker.enabled)
	{
		row_slider("##marker_size", "marker size", &g_Settings.hitmarker.size, 0.5f, 2.5f);
		gap();
		row_slider("##marker_time", "marker time", &g_Settings.hitmarker.duration, 0.2f, 1.5f);
		gap();
	}

	pad();
	ng::checkbox("hitsounds", &g_Settings.hitsound.enabled);
	gap();

	if (g_Settings.hitsound.enabled)
	{
		if (g_Settings.hitsound.index < 0 ||
		    g_Settings.hitsound.index >= Features::HitSounds::Count())
			g_Settings.hitsound.index = 0;

		if (row_select("##hitsound", "hitsound", &g_Settings.hitsound.index,
		               Features::HitSounds::Names(),
		               Features::HitSounds::Count()))
		{
			Features::HitSounds::Play(g_Settings.hitsound.index);
		}
		gap();

		row_slider("##hitsound_vol", "hitsound volume", &g_Settings.hitsound.volume,
		           0.0f, 100.0f);
		gap();
	}

	pad();
	ng::checkbox("hit data", &g_Settings.hitdata.enabled);
	gap();

	if (g_Settings.hitdata.enabled)
	{
		row_dropdown("##data_lines", "data lines", g_Settings.hitdata.modes,
		             Visuals::KillEffects::HitDataModeNames(),
		             Visuals::KillEffects::HitDataModeNameCount());
		gap();

		row_slider("##data_size", "data size", &g_Settings.hitdata.size, 10.0f, 28.0f);
		gap();
		row_slider("##data_time", "data time", &g_Settings.hitdata.duration, 0.4f, 2.5f);
		gap();
	}

	ng::child_end();
}
