#include "pch.h"
#include "Menu.h"
#include "colors/colors.h"
#include "resources/fonts/fonts.h"
#include "widgets/widgets.h"
#include "jewsploit_shell.h"
#include "jewsploit/players_ui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "misc/imgui_freetype.h"
#include "app/Settings.h"
#include "renderer/Renderer.h"
#include "features/visuals/ESP.h"
#include "features/visuals/HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include "features/games/ApocalypseRising.h"
#include "features/visuals/ESPPreview.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "app/Graphics.h"
#include "features/lua/LuaExecutor.h"
#include "features/lua/vm/LuaVM.h"
#include "features/lua/vm/LuaDrawing.h"
#include "features/visuals/ShaderChams.h"
#include "features/visuals/EngineChams.h"
#include "features/visuals/MeshDxShader.h"
#include "features/visuals/MeshChams.h"
#include "features/visuals/KillEffects.h"
#include "features/visuals/Crosshair.h"
#include "features/aim/Aim.h"
#include "features/aim/RaycastSilent.h"
#include "features/aim/MagicBullet.h"
#include "features/aim/ViewportSilent.h"
#include "features/misc/Misc.h"
#include "features/misc/HitSounds.h"
#include "features/misc/HitboxExpander.h"
#include "features/misc/PlayerAvatars.h"
#include "features/explorer/Explorer.h"
#include "features/mcp/McpBridge.h"
#include "core/config/Config.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace Cheat {
namespace GUI {

bool Menu::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    bool result = true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = nullptr;
    io.ConfigWindowsResizeFromEdges = true;

    fonts::load(io);
    menu::init();

    result = ImGui_ImplWin32_Init(hWnd);
    if (!result) return false;

    result = ImGui_ImplDX11_Init(pDevice, pDeviceContext);
    if (!result) return false;

    m_bInitialized = true;

    Cheat::Features::Explorer::Initialize();

    Cheat::Features::Misc::Start();
    Cheat::Visuals::EngineChams::Start();
    if (Cheat::g_Settings.misc.mcp)
        Cheat::Features::McpBridge::Start();

    return true;
}

// оверлей тик
void Menu::Render()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    {
        // весь гуй на выбранном шрифте из settings
        ImGuiIO& io = ImGui::GetIO();
        ImFont* ui_f = fonts::ui();
        if (ui_f)
            io.FontDefault = ui_f;
        ImGui::PushFont(ui_f, fonts::ui_size(ui_f));

        // игровые фичи только когда роблокс в фокусе
        if (Renderer::IsGameActive()) {
            Visuals::ESP::Render();
            // GPU mesh сразу после ESP — не ждать меню/explorer (меньше отставания от камеры)
            if (Cheat::Core::g_RenderTargetView)
                Cheat::Visuals::MeshDxShader::Flush(Cheat::Core::g_RenderTargetView);
            Features::HitboxExpander::Render();
            Features::Aim::Render();
            Visuals::KillEffects::Tick();
        }
        if (g_Settings.lua.executor)
        {
            Features::LuaExecutor::Initialize();
            if (Features::LuaVM::Ready())
                Features::LuaDrawing::Render();
        }
        DrawMenu();
        // всегда тикаем — float_panel сам анимирует close когда меню/тумблер выкл
        // баннеры тянем сразу, не ждать открытия players
        Features::PlayerAvatars::Tick();
        Features::Explorer::Render(1.f);
        Features::LuaExecutor::Render(1.f);
        ng_players::draw(1.f);
        // mcp тумблер из конфига / меню
        if (g_Settings.misc.mcp && !Features::McpBridge::Running())
            Features::McpBridge::Start();

        else if (!g_Settings.misc.mcp && Features::McpBridge::Running())
            Features::McpBridge::Stop();
        // прицел сверху всего
        Visuals::Crosshair::Render();

        ImGui::PopFont();
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool Menu::IsVisible()
{
    return m_bMenuVisible;
}

bool Menu::IsPointOverUI(float x, float y)
{
    if (!m_bInitialized)
        return false;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx || ctx->Windows.Size <= 0)
        return false;

    const ImVec2 p(x, y);
    for (int i = ctx->Windows.Size - 1; i >= 0; --i) {
        ImGuiWindow* w = ctx->Windows[i];
        if (!w || !w->Active || w->Hidden)
            continue;
        if (w->IsFallbackWindow)
            continue;
        if (w->Flags & ImGuiWindowFlags_Tooltip)
            continue;

        if (!(w->Flags & ImGuiWindowFlags_NoInputs)) {
            if (w->Rect().Contains(p))
                return true;
        }
    }
    return false;
}

bool Menu::ShouldCaptureMouse(float x, float y)
{
    if (!m_bInitialized) return false;

    // ватермарку таскаем даже с закрытым меню
    if (widgets::watermark_hit_test(x, y))
        return true;

    if (!m_bMenuVisible) return false;

    // меню открыто — всегда ловим мышь.
    // иначе дырка вокруг shell = WS_EX_TRANSPARENT и island сверху не кликается
    (void)x;
    (void)y;
    return true;
}

float Menu::DrawMenu()
{
    if (g_Settings.gui.watermark)
    {
        widgets::watermark(1.0f);
    }

    const bool was_open = menu::open;
    menu::draw();

    if (menu::open != m_bMenuVisible)
    {
        m_bMenuVisible = menu::open;
        Renderer::SetClickThrough(!m_bMenuVisible);

        if (m_bMenuVisible)
        {
            ClipCursor(nullptr);
            if (GetCapture())
                ReleaseCapture();

            if (!g_Settings.crosshair.enabled)
            {
                while (ShowCursor(TRUE) < 0) {}
            }
        }

        else
        {
            Renderer::SetTextInputFocus(false);

            // после open курсор free — вернуть клип/фокус в игру
            HWND game = Renderer::GetGameHwnd();
            if (game && IsWindow(game))
            {
                RECT cr{};
                if (GetClientRect(game, &cr))
                {
                    POINT tl{ cr.left, cr.top };
                    POINT br{ cr.right, cr.bottom };
                    ClientToScreen(game, &tl);
                    ClientToScreen(game, &br);
                    RECT clip{ tl.x, tl.y, br.x, br.y };
                    ClipCursor(&clip);
                }

                SetForegroundWindow(game);
            }

            // open делал ShowCursor(TRUE) — откат
            for (int i = 0; i < 8; ++i)
            {
                if (ShowCursor(FALSE) < 0)
                    break;
            }
        }
    }

    else if (menu::open != was_open)
    {
        m_bMenuVisible = menu::open;
        Renderer::SetClickThrough(!m_bMenuVisible);
    }

    m_menuAlpha = menu::open ? 1.f : 0.f;
    return 1.f;
}

void Menu::Shutdown()
{
    if (!m_bInitialized)
        return;

    menu::shutdown();

    Cheat::Features::Misc::Stop();
    Cheat::Visuals::EngineChams::Stop();

    Cheat::Features::RaycastSilent::Remove();
    Cheat::Features::MagicBullet::Remove();
    Cheat::Features::ViewportSilent::Shutdown();

    Cheat::Visuals::Crosshair::Shutdown();
    Cheat::Visuals::ESPPreview::Shutdown();
    Cheat::Features::Explorer::Shutdown();
    Cheat::Features::McpBridge::Stop();
    Cheat::Features::LuaExecutor::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Menu::InvalidateDeviceObjects()
{
    if (!m_bInitialized) return;
    ImGui_ImplDX11_InvalidateDeviceObjects();
}

void Menu::CreateDeviceObjects()
{
    if (!m_bInitialized) return;
    ImGui_ImplDX11_CreateDeviceObjects();
}

bool Menu::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!m_bInitialized)
        return false;
    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

}
}
