#include "pch.h"
#include "NoShadow.h"
#include "LightingInvalidate.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

namespace Cheat {
	namespace Features {

		bool NoShadow::Apply(std::uint64_t lighting)
		{
			static bool s_on = false;
			static std::uint8_t s_shadows = 1;
			static float s_bri = 1.0f;
			static float s_expo = 0.0f;
			static float s_env = 1.0f;
			static Color3 s_amb;
			static Color3 s_out;

			const auto& w = Cheat::g_Settings.world;
			bool dirty = false;

			if (w.no_shadow)
			{
				if (!s_on)
				{
					s_on = true;
					s_shadows = g_Memory.Read<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows);
					s_bri = g_Memory.Read<float>(lighting + Offsets::Lighting::Brightness);
					s_expo = g_Memory.Read<float>(lighting + Offsets::Lighting::ExposureCompensation);
					s_env = g_Memory.Read<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale);
					s_amb = g_Memory.Read<Color3>(lighting + Offsets::Lighting::Ambient);
					s_out = g_Memory.Read<Color3>(lighting + Offsets::Lighting::OutdoorAmbient);
				}

				float t = w.brightness;
				if (t < 0.f) t = 0.f;

				float bri = t * 2.5f;
				float expo = t * 0.35f;
				float amb = 1.f + t * 0.15f;
				float env = 1.f + t * 0.2f;

				dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, 0);
				dirty |= LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::Brightness, bri);
				dirty |= LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::ExposureCompensation, expo);
				dirty |= LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, env);
				dirty |= LightingInvalidate::write_if_changed<Color3>(lighting + Offsets::Lighting::Ambient, Color3(amb, amb, amb));
				dirty |= LightingInvalidate::write_if_changed<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, Color3(amb, amb, amb));
			}

			else if (s_on)
			{
				s_on = false;
				LightingInvalidate::write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_shadows);
				LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::Brightness, s_bri);
				LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::ExposureCompensation, s_expo);
				LightingInvalidate::write_if_changed<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, s_env);
				LightingInvalidate::write_if_changed<Color3>(lighting + Offsets::Lighting::Ambient, s_amb);
				LightingInvalidate::write_if_changed<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, s_out);
				dirty = true;
			}

			return dirty;
		}

	}
}
