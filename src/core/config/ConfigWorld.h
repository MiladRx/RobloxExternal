#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

inline void WriteWorld(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "world.no_shadow", s.world.no_shadow);
    PutFloat(out, "world.brightness", s.world.brightness);
    PutBool(out, "world.fog", s.world.fog);
    PutFloat(out, "world.fog_start", s.world.fog_start);
    PutFloat(out, "world.fog_end", s.world.fog_end);
    PutF4(out, "world.fog_color", s.world.fog_color);
    PutBool(out, "world.time_changer", s.world.time_changer);
    PutFloat(out, "world.clock_time", s.world.clock_time);
}

inline void ReadWorld(const KV& kv, Settings& s)
{
    GetBool(kv, "world.no_shadow", s.world.no_shadow);
    GetFloat(kv, "world.brightness", s.world.brightness);
    GetBool(kv, "world.fog", s.world.fog);
    GetFloat(kv, "world.fog_start", s.world.fog_start);
    GetFloat(kv, "world.fog_end", s.world.fog_end);
    GetF4(kv, "world.fog_color", s.world.fog_color);
    GetBool(kv, "world.time_changer", s.world.time_changer);
    GetFloat(kv, "world.clock_time", s.world.clock_time);
    if (s.world.clock_time < 0.f) s.world.clock_time = 0.f;
    if (s.world.clock_time > 24.f) s.world.clock_time = 24.f;
}

} // namespace detail
} // namespace Config
} // namespace Cheat
