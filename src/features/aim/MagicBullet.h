#pragma once
#include "core/roblox/math/Math.h"
#include <cstdint>

namespace Cheat {
    namespace Features {

        // own hook like RaycastSilent, wallbang always
        namespace MagicBullet {

            bool Install();
            void Remove();
            void Ensure(bool want = true);

            void SetActive(bool on, const Vector3& world_target = {});

            bool Ready();
            bool Aiming();

        }
    }
}
