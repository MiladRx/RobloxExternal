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

            void invalidate()
            {
				std::uint64_t rv = render_view();
				if (!rv)
					return;
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

            template <typename T>
            void force_write(std::uint64_t addr, T value)
            {
				if (g_Memory.IsValid(addr))
					g_Memory.Write<T>(addr, value);
            }

            void write_clock_time(std::uint64_t lighting, float t)
            {
				const std::uint64_t addr = lighting + Offsets::Lighting::ClockTime;
				const float  cur_f = g_Memory.Read<float>(addr);
				const double cur_d = g_Memory.Read<double>(addr);
				const bool f_ok = cur_f >= 0.f && cur_f <= 24.f;
				const bool d_ok = cur_d >= 0.0 && cur_d <= 24.0;

				if (d_ok && !f_ok)
					force_write<double>(addr, static_cast<double>(t));
				else
					force_write<float>(addr, t);
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
			static bool s_time_was_on = false;

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
				s_on = false;
				write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_shadows);
				write_if_changed<float>(lighting + Offsets::Lighting::Brightness, s_bri);
				write_if_changed<float>(lighting + Offsets::Lighting::ExposureCompensation, s_expo);
				write_if_changed<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, s_env);
				write_if_changed<Color3>(lighting + Offsets::Lighting::Ambient, s_amb);
				write_if_changed<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, s_out);
				dirty = true;
			}

			if (w.time_changer)
			{
				float t = w.clock_time;
				if (t < 0.f) t = 0.f;
				if (t > 24.f) t = 24.f;

				write_clock_time(lighting, t);

				constexpr float PI = 3.14159265f;
				const float sun_angle = (t / 24.f - 0.25f) * 2.f * PI;
				const float sun_y = std::sin(sun_angle);
				const float sun_x = 0.f;
				const float sun_z = -std::cos(sun_angle);

				const Vector3 sun_pos(sun_x, sun_y, sun_z);
				const Vector3 moon_pos(-sun_x, -sun_y, -sun_z);
				const Vector3 light_dir = (sun_y > 0.f) ? sun_pos : moon_pos;

				force_write<Vector3>(lighting + Offsets::Lighting::SunPosition, sun_pos);
				force_write<Vector3>(lighting + Offsets::Lighting::MoonPosition, moon_pos);
				force_write<Vector3>(lighting + Offsets::Lighting::LightDirection, light_dir);

				const float h = sun_y;
				Vector3 grad_top, grad_bot;
				float brightness = 2.f;
				Color3 outdoor;
				Color3 ambient;
				Color3 light_col;

				if (h > 0.15f) {
					grad_top = Vector3(0.35f, 0.65f, 0.95f);
					grad_bot = Vector3(0.75f, 0.85f, 0.95f);
					brightness = 2.0f;
					outdoor = Color3(0.6f + h * 0.1f, 0.6f + h * 0.1f, 0.62f + h * 0.1f);
					ambient = Color3(0.45f, 0.45f, 0.50f);
					light_col = Color3(1.f, 0.96f, 0.90f);
				} else if (h > 0.0f) {
					const float st = h / 0.15f;
					grad_top = Vector3(0.15f + st * 0.2f, 0.15f + st * 0.5f, 0.25f + st * 0.7f);
					grad_bot = Vector3(0.95f, 0.45f + st * 0.4f, 0.25f + st * 0.7f);
					brightness = 1.5f;
					outdoor = Color3(0.6f + h * 0.1f, 0.6f + h * 0.1f, 0.62f + h * 0.1f);
					ambient = Color3(0.35f + st * 0.1f, 0.30f + st * 0.1f, 0.35f + st * 0.1f);
					light_col = Color3(1.f, 0.55f + st * 0.4f, 0.35f + st * 0.4f);
				} else {
					float nt = (-h) / 0.3f;
					if (nt > 1.0f) nt = 1.0f;
					const float inv = 1.0f - nt;
					grad_top = Vector3(0.05f + inv * 0.1f, 0.05f + inv * 0.1f, 0.12f + inv * 0.13f);
					grad_bot = Vector3(0.08f + inv * 0.07f, 0.08f + inv * 0.07f, 0.15f + inv * 0.1f);
					brightness = 1.5f;
					outdoor = Color3(0.15f + inv * 0.2f, 0.15f + inv * 0.2f, 0.22f + inv * 0.2f);
					ambient = Color3(0.08f + inv * 0.12f, 0.08f + inv * 0.12f, 0.14f + inv * 0.12f);
					light_col = Color3(0.55f, 0.60f, 0.85f);
				}

				force_write<Vector3>(lighting + Offsets::Lighting::GradientTop, grad_top);
				force_write<Vector3>(lighting + Offsets::Lighting::GradientBottom, grad_bot);
				force_write<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, outdoor);
				force_write<Color3>(lighting + Offsets::Lighting::Ambient, ambient);
				force_write<Color3>(lighting + Offsets::Lighting::LightColor, light_col);
				force_write<float>(lighting + Offsets::Lighting::Brightness, brightness);

				dirty = true;
				s_time_was_on = true;
			}
			else if (s_time_was_on)
			{
				s_time_was_on = false;
				dirty = true;
			}

			if (w.fog)
			{
				float start = w.fog_start;
				if (start < 0.f) start = 0.f;
				if (start > 100.f) start = 100.f;

				float end = w.fog_end;
				if (end > 2000.f) end = 2000.f;
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
