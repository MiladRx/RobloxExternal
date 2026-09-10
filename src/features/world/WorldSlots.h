#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace WorldSlots {

std::uint64_t FindLighting();

// force_off = true on thread stop (rollback of backup)
void TickNoShadow(std::uint64_t lighting, bool force_off = false);
void TickTime(std::uint64_t lighting, bool force_off = false);
void TickAmbient(std::uint64_t lighting, bool force_off = false);
void TickOutdoor(std::uint64_t lighting, bool force_off = false);
void TickBrightness(std::uint64_t lighting, bool force_off = false);
void TickExposure(std::uint64_t lighting, bool force_off = false);
void TickLight(std::uint64_t lighting, bool force_off = false);
void TickFog(std::uint64_t lighting, bool force_off = false);
void TickEnv(std::uint64_t lighting, bool force_off = false);
void TickColorShift(std::uint64_t lighting, bool force_off = false);
void TickAtmosphere(std::uint64_t lighting, bool force_off = false);
void TickSky(std::uint64_t lighting, bool force_off = false);
void TickBloom(std::uint64_t lighting, bool force_off = false);
void TickColorCorr(std::uint64_t lighting, bool force_off = false);
void TickColorGrade(std::uint64_t lighting, bool force_off = false);
void TickDof(std::uint64_t lighting, bool force_off = false);
void TickTerrain(std::uint64_t lighting, bool force_off = false);
void TickSkyboxChanger(std::uint64_t lighting, bool force_off = false);

// preset names for the GUI — source of truth right here, next to the ids themselves
int SkyboxPresetCount();
const char* const* SkyboxPresetNames();

} // namespace WorldSlots
} // namespace Features
} // namespace Cheat
