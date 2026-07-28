#include "pch.h"
#include "KillEffects.h"
#include "features/aim/Aim.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"
#include "renderer/Renderer.h"
#include "app/Settings.h"
#include "features/misc/hitsounds/HitSounds.h"
#include "imgui.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace Cheat {
    namespace Visuals {
        namespace KillEffects {
            namespace {

#include "killfx/KillFxShared.h"
#include "killfx/PixelBurst.cpp"
#include "killfx/Confetti.cpp"
#include "killfx/Shatter.cpp"
#include "killfx/Hitmarker.cpp"
#include "killfx/HitText.cpp"

            }

            const char* const* EffectNames() { return k_fx_names; }
            const char* const* HitDataModeNames() { return k_hitdata_names; }

            void Tick()
            {
                bool any = g_Settings.killfx.enabled
                    || g_Settings.hitmarker.enabled
                    || g_Settings.hitdata.enabled
                    || g_Settings.hitsound.enabled;

                if (!any)
                {
                    g_pending.clear();
                    g_fx.clear();
                    g_markers.clear();
                    g_texts.clear();
                    g_watch = {};
                    g_lmb_was = false;
                    g_lmb_at = -1.0;
                    return;
                }
                if (!Globals::Workspace || !Globals::InstanceDataModel.address)
                    return;

                double now = ImGui::GetTime();
                float dt = ImGui::GetIO().DeltaTime;

                std::uint64_t target = Features::Aim::CurrentTarget();
                bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                if (lmb && !g_lmb_was)
                    g_lmb_at = now;
                g_lmb_was = lmb;

                bool recent_shot = lmb || (g_lmb_at >= 0.0 && (now - g_lmb_at) < 0.45);

                int aim_part = Features::Aim::CurrentAimPart();
                Vector3 aim_point = Features::Aim::CurrentAimPoint();

                if (g_Settings.hitmarker.enabled || g_Settings.hitdata.enabled
                    || g_Settings.hitsound.enabled)
                {
                    if (target)
                    {
                        if (g_watch.addr != target)
                        {
                            g_watch.addr = target;
                            g_watch.last_hp = -1.0f;
                            g_watch.last_fx = 0.0;
                        }
                        g_watch.part = aim_part;
                        g_watch.point = aim_point;
                        g_watch.keep_until = now + 0.85;
                    }

                    if (g_watch.addr && now <= g_watch.keep_until)
                    {
                        Vector3 pos = g_watch.point;
                        float hp = 0.0f;
                        bool found = false;
                        LookupTarget(g_watch.addr, pos, hp, found);

                        if (found)
                        {
                            if (g_watch.last_hp < 0.0f)
                            {
                                g_watch.last_hp = hp;
                            }

                            else
                            {
                                float drop = g_watch.last_hp - hp;
                                if (drop > 0.25f && recent_shot && (now - g_watch.last_fx) > 0.07)
                                {
                                    int part = g_watch.part;
                                    Vector3 hit_pos = g_watch.point;
                                    if (hit_pos.x == 0.f && hit_pos.y == 0.f && hit_pos.z == 0.f)
                                        hit_pos = pos;
                                    if (IsHead(part))
                                        hit_pos.y += 0.2f;

                                    else
                                        hit_pos.y += 0.08f;

                                    float dist = 0.0f;
                                    if (auto cam = Globals::Workspace->GetCurrentCamera())
                                        dist = (hit_pos - Camera(cam->address).GetPosition()).Length();

                                    if (g_Settings.hitmarker.enabled)
                                        SpawnMarker(hit_pos, IsHead(part));
                                    if (g_Settings.hitdata.enabled)
                                        SpawnHitTexts(hit_pos, part, drop, hp, dist);
                                    if (g_Settings.hitsound.enabled)
                                        Features::HitSounds::PlayCurrent();

                                    g_watch.last_fx = now;
                                }

                                g_watch.last_hp = hp;
                            }
                        }

                        else if (now > g_watch.keep_until)
                        {
                            g_watch = {};
                        }
                    }

                    else if (now > g_watch.keep_until)
                    {
                        g_watch = {};
                    }
                }

                else
                {
                    g_watch = {};
                }

                if (g_Settings.killfx.enabled && target && lmb)
                {
                    Vector3 pos{};
                    float hp = 100.0f;
                    bool found = false;
                    LookupTarget(target, pos, hp, found);
                    if (found)
                    {
                        bool have = false;
                        for (auto& p : g_pending)
                        {
                            if (p.addr == target)
                            {
                                p.click_at = now;
                                if (hp > 0.0f) p.was_alive = true;
                                if (pos.x != 0 || pos.y != 0 || pos.z != 0) p.pos = pos;
                                have = true;
                                break;
                            }
                        }
                        if (!have && hp > 0.0f)
                        {
                            PendingKill p;
                            p.addr = target;
                            p.pos = pos;
                            p.click_at = now;
                            p.was_alive = true;
                            g_pending.push_back(p);
                        }
                    }
                }

                if (g_Settings.killfx.enabled)
                {
                    for (size_t i = 0; i < g_pending.size(); )
                    {
                        auto& p = g_pending[i];
                        if (now - p.click_at > 2.0)
                        {
                            g_pending.erase(g_pending.begin() + (std::ptrdiff_t)i);
                            continue;
                        }

                        Vector3 pos = p.pos;
                        float hp = 0.0f;
                        bool found = false;
                        bool has_pos = LookupTarget(p.addr, pos, hp, found);
                        if (has_pos) p.pos = pos;

                        bool dead = !found || hp <= 0.0f;
                        if (p.was_alive && dead)
                        {
                            SpawnKill(g_Settings.killfx.effect, p.pos);
                            g_pending.erase(g_pending.begin() + (std::ptrdiff_t)i);
                            continue;
                        }
                        if (found && hp > 0.0f) p.was_alive = true;
                        ++i;
                    }
                }

                else
                {
                    g_pending.clear();
                }

                for (auto& fx : g_fx) fx.age += dt;
                for (auto& m : g_markers) m.age += dt;
                for (auto& t : g_texts) t.age += dt;

                g_fx.erase(std::remove_if(g_fx.begin(), g_fx.end(),
                    [](const KillFx& f) { return f.age >= f.life; }), g_fx.end());
                g_markers.erase(std::remove_if(g_markers.begin(), g_markers.end(),
                    [](const Marker& m) { return m.age >= m.life; }), g_markers.end());
                g_texts.erase(std::remove_if(g_texts.begin(), g_texts.end(),
                    [](const HitText& t) { return t.age >= t.life; }), g_texts.end());

                if (g_fx.empty() && g_markers.empty() && g_texts.empty())
                    return;

                auto cam_ptr = Globals::Workspace->GetCurrentCamera();
                if (!cam_ptr)
                    return;
                Camera camera(cam_ptr->address);
                Vector2 viewport = camera.GetViewportSize();

                float sx = 1.0f, sy = 1.0f;
                HWND oh = Renderer::GetHwnd();
                if (oh)
                {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr) && viewport.x > 1.f && viewport.y > 1.f)
                    {
                        sx = (float)(ocr.right - ocr.left) / viewport.x;
                        sy = (float)(ocr.bottom - ocr.top) / viewport.y;
                    }
                }

                static uintptr_t s_base = 0;
                if (!s_base) s_base = g_Memory.GetModuleBase();
                uintptr_t ve = g_Memory.Read<uintptr_t>(s_base + Offsets::VisualEngine::Pointer);
                Matrix4x4 vm = g_Memory.Read<Matrix4x4>(ve + Offsets::VisualEngine::ViewMatrix);

                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (!dl)
                    return;

                for (const auto& fx : g_fx)
                    DrawKillFx(dl, fx, vm, viewport, sx, sy);
                for (const auto& m : g_markers)
                    DrawHitmarker(dl, m, vm, viewport, sx, sy);
                for (const auto& t : g_texts)
                    DrawHitText(dl, t, vm, viewport, sx, sy);
            }

        }
    }
}
