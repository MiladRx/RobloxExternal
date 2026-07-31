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

			const auto& w = Cheat::g_Settings.world;
			bool dirty = false;

			// чисто тени
			if (w.no_shadow)
			{
				if (!s_on)
				{
					s_on = true;
					s_shadows = g_Memory.Read<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows);
				}

				dirty |= LightingInvalidate::write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, 0);
			}

			else if (s_on)
			{
				s_on = false;
				LightingInvalidate::write_if_changed<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_shadows);
				dirty = true;
			}

			return dirty;
		}

	}
}
