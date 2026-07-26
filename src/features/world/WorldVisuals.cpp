#include "pch.h"
#include "WorldVisuals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

namespace Cheat {
    namespace Features {

        namespace {
            std::uint64_t render_view()
            {
				static const uintptr_t base = g_Memory.GetModuleBase();
				if (!base)
					return 0;

				std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
				if (!g_Memory.IsValid(ve))
					return 0;

				std::uint64_t rv = g_Memory.Read<std::uint64_t>(ve + Offsets::VisualEngine::RenderView);
				return g_Memory.IsValid(rv) ? rv : 0;
            }

            // без этого лайтинг иногда тупит и не пересчитывает
            void invalidate()
            {
				std::uint64_t rv = render_view();
				if (rv)
					g_Memory.Write<std::uint8_t>(rv + Offsets::RenderView::LightingValid, 0);
            }

            template <typename T>
            bool write_if_changed(std::uint64_t addr, T value)
            {
				if (!g_Memory.IsValid(addr))
					return false;
				T cur = g_Memory.Read<T>(addr);
				if (cur == value)
					return false;
				g_Memory.Write<T>(addr, value);
				return true;
            }
        }

        void WorldVisuals::Apply(std::uint64_t lighting)
        {
			static bool s_on = false;
			static std::uint8_t s_shadows = 1;
			static float s_bri = 1.0f;
			static float s_expo = 0.0f;
			static float s_env = 1.0f;
			static Color3 s_amb;
			static Color3 s_out;

			if (!g_Memory.IsValid(lighting))
				return;

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

				dirty |= write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, 0);
				dirty |= write_if_changed<float>(lighting + Offsets::Lighting::Brightness, bri);
				dirty |= write_if_changed<float>(lighting + Offsets::Lighting::ExposureCompensation, expo);
				dirty |= write_if_changed<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, env);
				dirty |= write_if_changed<Color3>(lighting + Offsets::Lighting::Ambient, Color3(amb, amb, amb));
				dirty |= write_if_changed<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, Color3(amb, amb, amb));
			}

			else if (s_on)
			{
				// вернули как было
				s_on = false;
				write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_shadows);
				write_if_changed<float>(lighting + Offsets::Lighting::Brightness, s_bri);
				write_if_changed<float>(lighting + Offsets::Lighting::ExposureCompensation, s_expo);
				write_if_changed<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, s_env);
				write_if_changed<Color3>(lighting + Offsets::Lighting::Ambient, s_amb);
				write_if_changed<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, s_out);
				dirty = true;
			}

			if (w.fog)
			{
				float start = w.fog_start;
				if (start < 0.f) start = 0.f;

				float end = w.fog_end;
				if (end < start + 1.f) end = start + 1.f;

				dirty |= write_if_changed<float>(lighting + Offsets::Lighting::FogStart, start);
				dirty |= write_if_changed<float>(lighting + Offsets::Lighting::FogEnd, end);
				dirty |= write_if_changed<Color3>(lighting + Offsets::Lighting::FogColor,
					Color3(w.fog_color[0], w.fog_color[1], w.fog_color[2]));
			}

			if (dirty)
				invalidate();
        }

    }
}
