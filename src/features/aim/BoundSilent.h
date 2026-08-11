#pragma once
#include "core/roblox/math/Math.h"
#include <cstdint>

namespace Cheat {
    namespace Features {
        namespace BoundSilent {

            void Ensure(bool want);
            void Remove();
            void SetActive(bool on, const Vector3& world_target = {}, bool wallbang = false);

        }
    }
}
