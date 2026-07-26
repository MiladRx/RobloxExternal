#pragma once

#include <cstdint>
#include "core/player/PlayerHandler.h"
#include "core/roblox/math/Math.h"

namespace Cheat {
    namespace Features {
        namespace RaycastEngine {
            struct Result {
                bool  visible{ true };
                float coverage{ 1.0f };
            };

            bool IsEnabled();
            void Reset();
            void Tick();
            Result IsPlayerVisible(const PlayerCache& player, const Vector3& camera_pos);
        }
    }
}
