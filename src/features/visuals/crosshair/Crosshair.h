#pragma once

namespace Cheat {
namespace Visuals {
namespace Crosshair {

    void Render();
    void NotifyInactive(); // Roblox lost focus, restore cursor, don't touch the thread
    void Shutdown();       // full stop + restore the Windows cursor

} // namespace Crosshair
} // namespace Visuals
} // namespace Cheat
