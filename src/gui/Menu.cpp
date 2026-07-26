#include "pch.h"
#include "Menu.h"
#include "animation/animation.h"
#include "colors/colors.h"
#include "resources/fonts/fonts.h"
#include "widgets/widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "misc/imgui_freetype.h"
#include "app/Settings.h"
#include "renderer/Renderer.h"
#include "features/visuals/ESP.h"
#include "features/visuals/HavocWorldEsp.h"
#include "features/visuals/ESPPreview.h"
#include "features/lua/LuaExecutor.h"
#include "features/visuals/ShaderChams.h"
#include "features/visuals/EngineChams.h"
#include "features/visuals/KillEffects.h"
#include "features/visuals/Crosshair.h"
#include "features/aim/Aim.h"
#include "features/aim/RaycastSilent.h"
#include "features/aim/MagicBullet.h"
#include "features/aim/ViewportSilent.h"
#include "features/misc/Misc.h"
#include "features/misc/HitSounds.h"
#include "features/misc/HitboxExpander.h"
#include "features/explorer/Explorer.h"
#include "features/mcp/McpBridge.h"
#include "core/player/PlayerHandler.h"
#include "core/config/Config.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    void row_checkbox_color(const char* label, bool* value, float color[4], const char* color_id)
    {
        widgets::checkbox(label, value);
        const float row_y = widgets::color_picker_row_y();
        widgets::same_line_color_picker(row_y, 0, 1);
        widgets::color_edit4(color_id, color);
    }

    void sync_gui_theme()
    {
        auto& g = Cheat::g_Settings.gui;
        if (g.theme < 0 || g.theme >= colors::Theme_Count)
            g.theme = colors::Theme_Default;

        if (g.theme != colors::Theme_Custom)
        {
            colors::ApplyPreset(
                g.theme, g.accent, g.text_active, g.text_inactive,
                g.outer_border, g.inner_border, g.panel_fill,
                g.content_outer, g.content_inner, g.content_fill, g.child_fill);
        }

        colors::SyncFromSettings(
            g.accent, g.text_active, g.text_inactive,
            g.outer_border, g.inner_border, g.panel_fill,
            g.content_outer, g.content_inner, g.content_fill, g.child_fill);
    }

    void label_color_row(const char* label, float color[4], const char* id) {
        ImGui::SetCursorPosX(6.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

        const float row_y = ImGui::GetCursorPosY();
        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
        widgets::draw_outlined_text(
            ImGui::GetWindowDrawList(), font, fs,
            ImVec2(ImFloor(pos.x), ImFloor(pos.y)),
            colors::text_active_u32(), label);
        ImGui::Dummy(ImVec2(tsz.x, tsz.y));

        widgets::same_line_color_picker(row_y, 0, 1);
        widgets::color_edit4(id, color);
    }

    void theme_color_row(const char* label, float color[4], const char* id) {
        ImGui::SetCursorPosX(6.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
        widgets::draw_outlined_text(
            ImGui::GetWindowDrawList(), font, fs,
            ImVec2(ImFloor(pos.x), ImFloor(pos.y)),
            colors::text_active_u32(), label);
        ImGui::Dummy(ImVec2(tsz.x, tsz.y));

        const float row_y = widgets::color_picker_row_y();
        widgets::same_line_color_picker(row_y, 0, 1);
        if (widgets::color_edit4(id, color))
            Cheat::g_Settings.gui.theme = colors::Theme_Custom;
    }
}

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

    colors::apply_style();
    fonts::load(io);

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
        sync_gui_theme();
        ImGui::PushFont(fonts::ui(), fonts::ui_size());

        // игровые фичи только когда роблокс в фокусе
        if (Renderer::IsGameActive()) {
            Visuals::ESP::Render();
            Features::HitboxExpander::Render();
            Features::Aim::Render();
            Visuals::KillEffects::Tick();
        }
        const float menu_alpha = DrawMenu();
        Features::Explorer::Render(menu_alpha);
        Features::LuaExecutor::Render(menu_alpha);
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

    if (ImGui::GetIO().WantCaptureMouse) return true;
    return IsPointOverUI(x, y);
}

float Menu::DrawMenu()
{
    ImGuiIO& io = ImGui::GetIO();

    // меню открывается за ~0.3с
    float anim_dur = 0.30f;
    static bool insert_previous = false;
    static float animation_time = 0.30f;
    static bool animation_open_direction = true;
    static int menu_key = VK_DELETE;
    static int active_tab = 0;
    static ImVec2 s_menu_pos = {};
    static ImVec2 s_menu_size = {};

    bool typing = io.WantTextInput;
    bool insert_pressed = !typing && (GetAsyncKeyState(menu_key) & 0x8000) != 0;
    if (insert_pressed && !insert_previous)
    {
        m_bMenuVisible = !m_bMenuVisible;
        animation_time = 0.f;
        animation_open_direction = m_bMenuVisible;
        Renderer::SetClickThrough(!m_bMenuVisible);
        if (m_bMenuVisible)
        {
            ClipCursor(nullptr);
            if (GetCapture())
                ReleaseCapture();
            // прицел сам курсором рулит, системный не трогаем
            if (!g_Settings.crosshair.enabled)
            {
                while (ShowCursor(TRUE) < 0) {}
            }
        }

        else
        {
            Renderer::SetTextInputFocus(false);
        }
    }
    insert_previous = typing ? false : insert_pressed;

    if (animation_time < anim_dur)
    {
        animation_time += io.DeltaTime;
        if (animation_time > anim_dur)
            animation_time = anim_dur;
    }

    bool animation_running = animation_time < anim_dur;
    float progress = animation_time / anim_dur;
    if (progress < 0.f) progress = 0.f;
    if (progress > 1.f) progress = 1.f;

    float window_alpha = 0.f;
    if (animation_open_direction)
        window_alpha = animation::ease_cubic_out(progress);

    else
        window_alpha = 1.f - animation::ease_cubic_in(progress);

    m_menuAlpha = window_alpha;

    if (g_Settings.gui.watermark) {
        // альфу меню не мешаем, ватермарка всегда полная
        widgets::watermark(1.0f);
    }

    if (!m_bMenuVisible && !animation_running && window_alpha <= 0.0f) {
        return 0.0f;
    }

    ImVec2 menu_sz(720.f, 580.f);

    ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(menu_sz, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660.f, 440.f), ImVec2(1100.f, 960.f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    if (ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        s_menu_pos = ImGui::GetWindowPos();
        s_menu_size = ImGui::GetWindowSize();

        {
            // края для ресайза
            float grip = 6.f;
            float min_w = 660.f, max_w = 1100.f;
            float min_h = 440.f, max_h = 960.f;
            ImVec2 sz = s_menu_size;

            ImGui::SetCursorPos(ImVec2(sz.x - grip, 0.f));
            ImGui::InvisibleButton("##rzr", ImVec2(grip, sz.y - grip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActive())
            {
                float nw = sz.x + io.MouseDelta.x;
                if (nw < min_w) nw = min_w;
                if (nw > max_w) nw = max_w;
                ImGui::SetWindowSize(ImVec2(nw, sz.y));
            }

            ImGui::SetCursorPos(ImVec2(0.f, sz.y - grip));
            ImGui::InvisibleButton("##rzb", ImVec2(sz.x - grip, grip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsItemActive())
            {
                float nh = sz.y + io.MouseDelta.y;
                if (nh < min_h) nh = min_h;
                if (nh > max_h) nh = max_h;
                ImGui::SetWindowSize(ImVec2(sz.x, nh));
            }

            ImGui::SetCursorPos(ImVec2(sz.x - grip, sz.y - grip));
            ImGui::InvisibleButton("##rzc", ImVec2(grip, grip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            if (ImGui::IsItemActive())
            {
                float nw = sz.x + io.MouseDelta.x;
                float nh = sz.y + io.MouseDelta.y;
                if (nw < min_w) nw = min_w;
                if (nw > max_w) nw = max_w;
                if (nh < min_h) nh = min_h;
                if (nh > max_h) nh = max_h;
                ImGui::SetWindowSize(ImVec2(nw, nh));
            }
        }
        const ImVec2 window_pos = s_menu_pos;
        const ImVec2 window_size = s_menu_size;
        colors::draw_panel_background(window_alpha);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float border = 2.f;
        ImVec2 title_pad(12.f, 12.f);
        ImVec2 text_pos(
            window_pos.x + border + title_pad.x,
            window_pos.y + border + title_pad.y);

        ImU32 text_color = colors::text_active_u32();
        ImU32 accent_color = colors::accent_u32();
        ImU32 highlight_color = ImGui::ColorConvertFloat4ToU32(
            ImLerp(colors::text_active, colors::accent, 0.35f));
        float font_size = fonts::ui_size(fonts::ui_bold());

        widgets::text_span title_spans[] = {
            {"jew", accent_color},
            {"sploit", text_color},
        };

        float title_phase = (float)ImGui::GetTime() * 2.4f;
        float title_wavelength = 90.f;
        float title_intro_offset = (1.f - window_alpha) * 6.f;
        ImVec2 title_pos = text_pos;
        title_pos.y -= title_intro_offset;

        widgets::draw_outlined_text_spans_shimmer(
            draw_list,
            fonts::ui_bold(),
            font_size,
            title_pos,
            title_spans,
            IM_ARRAYSIZE(title_spans),
            highlight_color,
            title_phase,
            title_wavelength);

        // панель + табы
        float content_margin_x = 12.f;
        float content_top = 36.f;
        float content_margin_bottom = 12.f;
        float tab_w = 100.f;
        float tab_h = 22.f;
        float tab_spacing = 3.f;
        float panel_inset = 2.f;
        ImVec2 content_pad(8.f, 8.f);
        float min_content_w = 360.f;
        float min_content_h = 300.f;

        float content_w = window_size.x - content_margin_x * 2.f;
        if (content_w < min_content_w) content_w = min_content_w;
        float content_h = window_size.y - content_top - content_margin_bottom;
        if (content_h < min_content_h) content_h = min_content_h;

        ImVec2 panel_min(
            window_pos.x + content_margin_x,
            window_pos.y + content_top);
        ImVec2 panel_max(panel_min.x + content_w, panel_min.y + content_h);
        float tabs_total_width = tab_w * 5.f + tab_spacing * 4.f;
        float tab_start_x = panel_max.x - tabs_total_width;
        float tab_top = panel_min.y - tab_h;
        const char* tab_labels[] = { "esp", "players", "settings", "aim", "misc" };

        ImU32 panel_fill = ImGui::GetColorU32(colors::content_fill);
        ImU32 panel_outer = ImGui::GetColorU32(colors::content_outer_border);
        ImU32 panel_inner = ImGui::GetColorU32(colors::content_inner_border);

        float tab_font_size = fonts::ui_size(fonts::ui_bold());

        active_tab = widgets::draw_tab_bar(
            draw_list,
            panel_min,
            panel_max,
            tab_start_x,
            tab_top,
            active_tab,
            tab_labels,
            IM_ARRAYSIZE(tab_labels),
            tab_w,
            tab_h,
            tab_spacing,
            fonts::ui_bold(),
            fonts::ui_bold(),
            tab_font_size,
            panel_fill,
            panel_outer,
            panel_inner);

        ImVec2 child_size(
            content_w - panel_inset * 2.f - content_pad.x * 2.f,
            content_h - panel_inset * 2.f - content_pad.y * 2.f);

        ImGui::SetCursorPos(ImVec2(
            content_margin_x + panel_inset + content_pad.x,
            content_top + panel_inset + content_pad.y));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        if (ImGui::BeginChild(
            "menu_content",
            child_size,
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            float side_child_w_min = 150.f;
            float side_child_h_min = 200.f;
            float child_margin = 4.f;
            float side_child_gap = 8.f;

            ImVec2 content_avail = ImGui::GetContentRegionAvail();
            float inner_w = content_avail.x - child_margin * 2.f;
            float side_child_h = content_avail.y - child_margin * 2.f;
            if (side_child_h < side_child_h_min) side_child_h = side_child_h_min;

            const float side_title_size = fonts::ui_size(fonts::ui_bold());

            auto make_side_child_size = [&](int columns) -> ImVec2 {
                int cols = columns;
                if (cols < 1) cols = 1;
                if (cols > 2) cols = 2;
                float gaps = side_child_gap * (float)(cols - 1);
                float column_w = (inner_w - gaps) / (float)cols;
                if (column_w < side_child_w_min) column_w = side_child_w_min;
                return ImVec2(column_w, side_child_h);
            };

            auto draw_side_child = [&](
                const char* id,
                const char* title,
                const ImVec2& cursor_pos,
                const ImVec2& size) {
                ImGui::SetCursorPos(cursor_pos);
                return widgets::begin_child_panel(
                    id,
                    size,
                    title,
                    fonts::ui_bold(),
                    side_title_size,
                    nullptr,
                    nullptr,
                    nullptr);
            };

            const ImVec2 full_child_size = make_side_child_size(1);
            const ImVec2 full_child_pos(child_margin, child_margin);

            if (active_tab == 0)
            {
                const ImVec2 side_child_size = make_side_child_size(2);
                const ImVec2 left_child_pos(child_margin, child_margin);
                const ImVec2 right_child_pos(
                    child_margin + side_child_size.x + side_child_gap,
                    child_margin);

                if (draw_side_child("esp_main", "esp", left_child_pos, side_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::checkbox("enabled", &g_Settings.esp.enabled);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("draw local", &g_Settings.esp.draw_local);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("bounding box", &g_Settings.esp.box, g_Settings.esp.box_color, "esp_box_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("name", &g_Settings.esp.name, g_Settings.esp.name_color, "esp_name_color");

                    if (g_Settings.esp.name) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        static const char* k_name_modes[] = { "display name", "username" };
                        widgets::combo("name type", &g_Settings.esp.name_mode, k_name_modes, 2);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("skeleton", &g_Settings.esp.skeleton, g_Settings.esp.skeleton_color, "esp_skeleton_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("chams", &g_Settings.esp.chams);

                    if (g_Settings.esp.chams_mode != 3 && g_Settings.esp.chams_mode != 4) {
                        const float row_y = widgets::color_picker_row_y();
                        widgets::same_line_color_picker(row_y, 1, 2);
                        widgets::color_edit4("esp_chams_outline_color", g_Settings.esp.chams_outline_color);
                        widgets::same_line_color_picker(row_y, 0, 2);
                        widgets::color_edit4("esp_chams_fill_color", g_Settings.esp.chams_fill_color);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_chams_modes[] = {
                            "box", "box filled", "clipper", "shader", "engine"
                        };
                        widgets::combo("chams mode", &g_Settings.esp.chams_mode, k_chams_modes, 5);
                    }

                    if (g_Settings.esp.chams_mode == 3) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::combo("shader", &g_Settings.esp.chams_shader,
                                       Visuals::ShaderChams::StyleNames(),
                                       Visuals::ShaderChams::StyleNameCount());
                    }

                    if (g_Settings.esp.chams_mode == 4) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* engine_styles[] = { "default", "ghost", "wireframe" };
                            widgets::combo("engine style", &g_Settings.esp.engine_chams_style,
                                           engine_styles, 3);
                        }

                        if (g_Settings.esp.engine_chams_style == 1) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            static const char* engine_colors[] = {
                                "red", "green", "orange", "blue", "pink", "cyan", "white"
                            };
                            widgets::combo("ghost color", &g_Settings.esp.engine_ghost_color_idx,
                                           engine_colors, 7);
                        }
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("health bar", &g_Settings.esp.healthbar);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("health text", &g_Settings.esp.health_text);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("distance", &g_Settings.esp.distance, g_Settings.esp.distance_color, "esp_distance_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("tool", &g_Settings.esp.tool, g_Settings.esp.tool_color, "esp_tool_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("flags", &g_Settings.esp.flags);

                    const bool havoc_place = Visuals::HavocWorldEsp::IsActivePlace();
                    if (havoc_place)
                    {
                        g_Settings.esp.body_corpse = false;

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("dead check", &g_Settings.esp.dead_check);

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("bots", &g_Settings.esp.bots);
                        if (g_Settings.esp.bots) {
                            auto& be = g_Settings.esp.bot_esp;

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("bot box", &be[Cheat::Settings::BOT_BOX],
                                               g_Settings.esp.bot_box_color, "esp_bot_box_color");

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("bot name", &be[Cheat::Settings::BOT_NAME],
                                               g_Settings.esp.bot_name_color, "esp_bot_name_color");

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("bot skeleton", &be[Cheat::Settings::BOT_SKELETON],
                                               g_Settings.esp.bot_skeleton_color, "esp_bot_skel_color");

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("bot chams", &be[Cheat::Settings::BOT_CHAMS]);

                            if (be[Cheat::Settings::BOT_CHAMS] &&
                                g_Settings.esp.bot_chams_mode != 3) {
                                const float row_y = widgets::color_picker_row_y();
                                widgets::same_line_color_picker(row_y, 1, 2);
                                widgets::color_edit4("esp_bot_chams_outline",
                                                     g_Settings.esp.bot_chams_outline_color);
                                widgets::same_line_color_picker(row_y, 0, 2);
                                widgets::color_edit4("esp_bot_chams_fill",
                                                     g_Settings.esp.bot_chams_fill_color);
                            }

                            if (be[Cheat::Settings::BOT_CHAMS]) {
                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                                {
                                    static const char* k_bot_chams_modes[] = {
                                        "box", "box filled", "clipper", "shader"
                                    };
                                    if (g_Settings.esp.bot_chams_mode > 3)
                                        g_Settings.esp.bot_chams_mode = 3;
                                    widgets::combo("bot chams mode", &g_Settings.esp.bot_chams_mode,
                                                   k_bot_chams_modes, 4);
                                }

                                if (g_Settings.esp.bot_chams_mode == 3) {
                                    ImGui::SetCursorPosX(6.0f);
                                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                                    widgets::combo("bot shader", &g_Settings.esp.bot_chams_shader,
                                                   Visuals::ShaderChams::StyleNames(),
                                                   Visuals::ShaderChams::StyleNameCount());
                                }
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("bot health bar", &be[Cheat::Settings::BOT_HEALTHBAR]);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("bot health text", &be[Cheat::Settings::BOT_HEALTH_TEXT]);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("bot distance", &be[Cheat::Settings::BOT_DISTANCE],
                                               g_Settings.esp.bot_distance_color, "esp_bot_dist_color");

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("bot tool", &be[Cheat::Settings::BOT_TOOL],
                                               g_Settings.esp.bot_tool_color, "esp_bot_tool_color");

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("bot flags", &be[Cheat::Settings::BOT_FLAGS]);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        row_checkbox_color("corpses", &g_Settings.esp.corpses,
                                           g_Settings.esp.corpse_color, "esp_corpse_color");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        row_checkbox_color("ground loot", &g_Settings.esp.ground_loot,
                                           g_Settings.esp.ground_loot_color, "esp_ground_loot_color");
                        if (g_Settings.esp.ground_loot) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("loot chams", &g_Settings.esp.loot_chams);

                            if (g_Settings.esp.loot_chams) {
                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                                widgets::combo("loot shader", &g_Settings.esp.loot_chams_shader,
                                               Visuals::ShaderChams::StyleNames(),
                                               Visuals::ShaderChams::StyleNameCount());
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            {
                                static const char* k_loot_filters[] = {
                                    "weapons", "mags", "ammo", "attachments", "medical",
                                    "valuables", "tools", "electronics", "households",
                                    "documents", "other"
                                };
                                widgets::multi_combo(
                                    "loot filters", g_Settings.esp.loot_filter,
                                    k_loot_filters, Cheat::Settings::LOOT_FILTER_COUNT);
                            }
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        row_checkbox_color("containers", &g_Settings.esp.containers,
                                           g_Settings.esp.containers_color, "esp_containers_color");
                        if (g_Settings.esp.containers)
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("container chams", &g_Settings.esp.containers_chams);

                            if (g_Settings.esp.containers_chams)
                            {
                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                                widgets::combo("crate shader", &g_Settings.esp.containers_chams_shader,
                                               Visuals::ShaderChams::StyleNames(),
                                               Visuals::ShaderChams::StyleNameCount());
                            }
                        }
                    }

                    else
                    {
                        g_Settings.esp.bots = true;
                        g_Settings.esp.corpses = false;
                        g_Settings.esp.ground_loot = false;
                        g_Settings.esp.containers = false;

                        if (!g_Settings.esp.body_corpse)
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            widgets::checkbox("dead check", &g_Settings.esp.dead_check);
                        }

                        else
                        {
                            g_Settings.esp.dead_check = false;
                        }

                        if (!g_Settings.esp.dead_check)
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                            row_checkbox_color("body corpse", &g_Settings.esp.body_corpse,
                                               g_Settings.esp.corpse_color, "esp_corpse_color");
                        }

                        else
                        {
                            g_Settings.esp.body_corpse = false;
                        }
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("tracer", &g_Settings.esp.tracer,
                                       g_Settings.esp.tracer_color, "esp_tracer_color");

                    if (g_Settings.esp.tracer) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        static const char* k_tracer_origin[] = {
                            "bottom", "center", "mouse", "top"
                        };
                        widgets::combo("tracer origin", &g_Settings.esp.tracer_origin,
                                       k_tracer_origin, 4);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("offscreen arrows", &g_Settings.esp.offscreen_arrows,
                                       g_Settings.esp.arrow_color, "esp_arrow_color");

                    if (g_Settings.esp.offscreen_arrows) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* k_arrow_info[] = {
                                "name", "distance", "health", "tool"
                            };
                            widgets::multi_combo(
                                "arrow info", g_Settings.esp.arrow_info,
                                k_arrow_info, Cheat::Settings::ARROW_INFO_COUNT);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("arrow size", &g_Settings.esp.arrow_size,
                                              6.0f, 32.0f, "%.0f");
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("arrow radius", &g_Settings.esp.arrow_radius,
                                              40.0f, 500.0f, "%.0f");
                    }

                }
                widgets::end_child_panel();

                if (draw_side_child("esp_settings", "settings", right_child_pos, side_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    {
                        static const char* k_esp_fonts[] = {
                            "imgui", "tahoma bold", "proggy clean", "visitor", "verdana"
                        };
                        widgets::combo("esp font", &g_Settings.esp.font, k_esp_fonts, 5);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("esp font size", &g_Settings.esp.font_size, 8.0f, 24.0f, "%.0fpx");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_box_modes[] = { "bounding", "corner", "3d" };
                        widgets::combo("box style", &g_Settings.esp.box_mode, k_box_modes, 3);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    if (Visuals::HavocWorldEsp::IsActivePlace())
                    {
                        // havoc custom: всегда метры, кап 400
                        ImGui::TextUnformatted("distance: meters (max 400)");
                    }

                    else
                    {
                        static const char* k_dist_units[] = { "studs", "meters" };
                        const int prev_unit = g_Settings.esp.distance_unit;
                        widgets::combo("distance unit", &g_Settings.esp.distance_unit, k_dist_units, 2);
                        if (prev_unit != g_Settings.esp.distance_unit)
                        {
                            float k = 0.28f;
                            if (g_Settings.esp.distance_unit == 1)
                                g_Settings.esp.max_distance *= k;

                            else if (k > 0.f)
                                g_Settings.esp.max_distance /= k;
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::checkbox("distance check", &g_Settings.esp.distance_check);

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            bool meters = g_Settings.esp.distance_unit == 1;
                            float lo = meters ? 10.f : 50.f;
                            float hi = meters ? 1400.f : 5000.f;
                            float d = g_Settings.esp.max_distance;
                            if (d < lo) d = lo;
                            if (d > hi) d = hi;
                            g_Settings.esp.max_distance = d;
                            widgets::slider_float(
                                meters ? "max distance (meters)" : "max distance (studs)",
                                &g_Settings.esp.max_distance, lo, hi, "%.0f");
                        }
                    }

                }
                widgets::end_child_panel();
            }

            else if (active_tab == 1)
            {
                if (draw_side_child("players_main", "players", full_child_pos, full_child_size))
                {
                    if (PlayerHandler::GetPlayerCount() == 0)
                    {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::TextUnformatted("no players cached");
                    }

                    else
                    {
                        PlayerHandler::ForEachPlayer([](const PlayerCache& player) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::TextUnformatted(player.name.empty() ? "unknown" : player.name.c_str());
                        });
                    }
                }
                widgets::end_child_panel();
            }

            else if (active_tab == 2)
            {
                const ImVec2 set_child_size = make_side_child_size(2);
                const ImVec2 set_left_pos(child_margin, child_margin);
                const ImVec2 set_right_pos(
                    set_left_pos.x + set_child_size.x + side_child_gap,
                    child_margin);

                if (draw_side_child("settings_main", "menu", set_left_pos, set_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                    widgets::keybind("menu key", &menu_key);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("watermark", &g_Settings.gui.watermark);

                    if (g_Settings.gui.watermark) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        static const char* k_wm_fields[] = {
                            "build", "player", "place id", "game id", "time", "fps"
                        };
                        widgets::multi_combo(
                            "watermark info",
                            g_Settings.gui.watermark_fields,
                            k_wm_fields,
                            Cheat::Settings::WM_FIELD_COUNT);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("raycast engine", &g_Settings.misc.raycast_engine);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("explorer", &g_Settings.misc.explorer);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        const bool prev = g_Settings.misc.mcp;
                        widgets::checkbox("mcp", &g_Settings.misc.mcp);
                        if (g_Settings.misc.mcp != prev) {
                            if (g_Settings.misc.mcp)
                                Features::McpBridge::Start();
                            else
                                Features::McpBridge::Stop();
                        }
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("custom support", &g_Settings.misc.custom_support);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("lua executor", &g_Settings.lua.executor);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_gui_fonts[] = {
                            "imgui", "tahoma bold", "proggy clean", "visitor", "verdana"
                        };
                        widgets::combo("font", &g_Settings.gui.font, k_gui_fonts, 5);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        auto& gui = g_Settings.gui;
                        static int s_last_theme = -1;
                        widgets::combo("theme", &gui.theme,
                                       colors::ThemeNames(), colors::ThemeNameCount());

                        if (gui.theme != s_last_theme) {
                            s_last_theme = gui.theme;
                            if (gui.theme != colors::Theme_Custom) {
                                colors::ApplyPreset(
                                    gui.theme,
                                    gui.accent, gui.text_active, gui.text_inactive,
                                    gui.outer_border, gui.inner_border, gui.panel_fill,
                                    gui.content_outer, gui.content_inner,
                                    gui.content_fill, gui.child_fill);
                            }
                            sync_gui_theme();
                        }
                    }

                    theme_color_row("accent",         g_Settings.gui.accent,         "gui_accent");
                    theme_color_row("text",           g_Settings.gui.text_active,    "gui_text");
                    theme_color_row("text dim",       g_Settings.gui.text_inactive,  "gui_text_dim");
                    theme_color_row("outer border",   g_Settings.gui.outer_border,   "gui_outer");
                    theme_color_row("inner border",   g_Settings.gui.inner_border,   "gui_inner");
                    theme_color_row("panel",          g_Settings.gui.panel_fill,     "gui_panel");
                    theme_color_row("content border", g_Settings.gui.content_outer,  "gui_c_outer");
                    theme_color_row("content inline", g_Settings.gui.content_inner,  "gui_c_inner");
                    theme_color_row("content",        g_Settings.gui.content_fill,   "gui_content");
                    theme_color_row("child",          g_Settings.gui.child_fill,     "gui_child");
                }
                widgets::end_child_panel();

                if (draw_side_child("settings_configs", "configs", set_right_pos, set_child_size)) {
                    static char s_cfg_name[64] = "default";
                    static int  s_cfg_sel = -1;
                    static std::vector<std::string> s_cfg_list;
                    static float s_cfg_refresh_at = 0.f;

                    const float now = (float)ImGui::GetTime();
                    if (now >= s_cfg_refresh_at) {
                        s_cfg_list = Cheat::Config::List();
                        s_cfg_refresh_at = now + 1.0f;
                        if (s_cfg_sel >= (int)s_cfg_list.size())
                            s_cfg_sel = (int)s_cfg_list.size() - 1;
                    }

                    float cfg_pad_x = 6.f;
                    float btn_gap = 4.f;
                    ImGui::SetCursorPosX(cfg_pad_x);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f);
                    float field_w = ImGui::GetContentRegionAvail().x - cfg_pad_x;
                    if (field_w < 60.f) field_w = 60.f;
                    widgets::input_text(
                        "##cfg_name", "config name", s_cfg_name, IM_ARRAYSIZE(s_cfg_name),
                        field_w, 0);

                    {
                        const float btn_w = (field_w - btn_gap * 2.0f) / 3.0f;
                        ImGui::SetCursorPosX(cfg_pad_x);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        if (widgets::button("save", ImVec2(btn_w, 0.0f))) {
                            // сейв в выбранный конфиг (перезапись)
                            const char* name = s_cfg_name;
                            if (s_cfg_sel >= 0 && s_cfg_sel < (int)s_cfg_list.size())
                                name = s_cfg_list[s_cfg_sel].c_str();
                            if (name && name[0] && Cheat::Config::Save(name)) {
                                std::snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
                                s_cfg_list = Cheat::Config::List();
                                s_cfg_refresh_at = now + 1.0f;
                                s_cfg_sel = -1;
                                for (int i = 0; i < (int)s_cfg_list.size(); ++i) {
                                    if (s_cfg_list[i] == name) {
                                        s_cfg_sel = i;
                                        break;
                                    }
                                }
                            }
                        }
                        ImGui::SameLine(0.0f, btn_gap);
                        if (widgets::button("load", ImVec2(btn_w, 0.0f))) {
                            const char* name = s_cfg_name;
                            if (s_cfg_sel >= 0 && s_cfg_sel < (int)s_cfg_list.size())
                                name = s_cfg_list[s_cfg_sel].c_str();
                            if (name && name[0] && Cheat::Config::Load(name)) {
                                std::snprintf(s_cfg_name, sizeof(s_cfg_name), "%s", name);
                                sync_gui_theme();
                            }
                        }
                        ImGui::SameLine(0.0f, btn_gap);
                        if (widgets::button("delete", ImVec2(btn_w, 0.0f))) {
                            const char* name = s_cfg_name;
                            if (s_cfg_sel >= 0 && s_cfg_sel < (int)s_cfg_list.size())
                                name = s_cfg_list[s_cfg_sel].c_str();
                            if (name && name[0] && Cheat::Config::Remove(name)) {
                                s_cfg_list = Cheat::Config::List();
                                s_cfg_sel = -1;
                                s_cfg_refresh_at = now + 1.0f;
                            }
                        }
                    }

                    ImGui::SetCursorPosX(cfg_pad_x);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                    const float list_h = ImMax(80.0f, ImGui::GetContentRegionAvail().y - 6.0f);
                    const float tf = fonts::ui_size(fonts::ui_bold());

                    if (widgets::begin_child_panel(
                            "cfg_list_panel", ImVec2(field_w, list_h),
                            "saved", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
                    {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                        if (ImGui::BeginChild(
                                "##cfg_scroll",
                                ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
                                false))
                        {
                            ImFont* list_font = fonts::ui();
                            const float list_fs = fonts::ui_size(list_font);
                            float row_h = 18.f;

                            if (s_cfg_list.empty()) {
                                ImGui::SetCursorPos(ImVec2(6.0f, 4.0f));
                                widgets::draw_outlined_text(
                                    ImGui::GetWindowDrawList(), list_font, list_fs,
                                    ImGui::GetCursorScreenPos(),
                                    colors::text_inactive_u32(), "no configs");
                                ImGui::Dummy(ImVec2(0.0f, row_h));
                            }

                            for (int i = 0; i < (int)s_cfg_list.size(); ++i) {
                                ImGui::PushID(i);
                                ImGui::SetCursorPosX(4.0f);

                                const ImVec2 row_pos = ImGui::GetCursorScreenPos();
                                const float row_w = ImGui::GetContentRegionAvail().x - 4.0f;
                                char row_id[32];
                                std::snprintf(row_id, sizeof(row_id), "cfg_row_%d", i);
                                if (ImGui::InvisibleButton(row_id, ImVec2(row_w, row_h))) {
                                    s_cfg_sel = i;
                                    std::snprintf(s_cfg_name, sizeof(s_cfg_name), "%s",
                                                  s_cfg_list[i].c_str());
                                }

                                const bool selected = (s_cfg_sel == i);
                                const bool hovered = ImGui::IsItemHovered();
                                ImU32 col = colors::text_inactive_u32();
                                if (selected)
                                    col = colors::accent_u32();
                                else if (hovered)
                                    col = colors::text_active_u32();

                                const char* name = s_cfg_list[i].c_str();
                                const ImVec2 ts = list_font->CalcTextSizeA(
                                    list_fs, FLT_MAX, 0.0f, name);
                                const float text_y = ImFloor(row_pos.y + (row_h - ts.y) * 0.5f);
                                if (selected) {
                                    widgets::draw_outlined_text(
                                        ImGui::GetWindowDrawList(), list_font, list_fs,
                                        ImVec2(ImFloor(row_pos.x + 4.0f), text_y),
                                        colors::accent_u32(), ">");
                                }
                                widgets::draw_outlined_text(
                                    ImGui::GetWindowDrawList(), list_font, list_fs,
                                    ImVec2(ImFloor(row_pos.x + (selected ? 14.0f : 6.0f)), text_y),
                                    col, name);

                                ImGui::PopID();
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                    widgets::end_child_panel();
                }
                widgets::end_child_panel();
            }

            else if (active_tab == 3)
            {
                const ImVec2 aim_child_size = make_side_child_size(2);
                const ImVec2 aim_left_pos(child_margin, child_margin);
                const ImVec2 aim_right_pos(
                    aim_left_pos.x + aim_child_size.x + side_child_gap,
                    child_margin);

                // вкладка аима, методы сайлента тут
                Cheat::Settings::AimbotConfig& cfg = g_Settings.aim.active();

                if (draw_side_child("aim_main", "aim", aim_left_pos, aim_child_size))
                {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                    widgets::keybind("aim key", &g_Settings.aim.bind, &g_Settings.aim.bind_mode);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    {
                        static const char* k_types[] = { "mouse", "camera", "silent" };
                        widgets::combo("type", &g_Settings.aim.type, k_types, 3);
                    }

                    if (g_Settings.aim.type == 2)
                    {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        {
                            static const char* k_silent[] = {
                                "viewport", "mouse", "raycast", "magic bullet"
                            };
                            widgets::combo("silent method", &g_Settings.aim.silent_method,
                                k_silent, Cheat::Settings::SILENT_METHOD_COUNT);
                        }

                        if (g_Settings.aim.silent_method == Cheat::Settings::SILENT_RAYCAST)
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            widgets::checkbox_keybind(
                                "force magic bullet",
                                &g_Settings.aim.force_magic_bullet,
                                &g_Settings.aim.force_magic_key,
                                &g_Settings.aim.force_magic_mode);
                        }
                    }

                    else
                    {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("smoothness x", &cfg.smooth_x, 0.1f, 5.0f, "%.1f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("smoothness y", &cfg.smooth_y, 0.1f, 5.0f, "%.1f");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    row_checkbox_color("fov check", &cfg.fov_enabled, cfg.fov_color, "aim_fov_color");

                    if (cfg.fov_enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* k_fov_style[] = { "circle", "filled" };
                            widgets::combo("fov style", &cfg.fov_style, k_fov_style, 2);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        row_checkbox_color("fov outline", &cfg.fov_outline,
                                           cfg.fov_outline_color, "aim_fov_outline_color");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::slider_float("fov size", &cfg.fov_size, 10.0f, 600.0f, "%.0f");
                    }

                    // откуда аим/трейсер, не зависит от фов круга
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_pos[] = { "center", "mouse" };
                        widgets::combo("aim pos", &cfg.fov_position, k_pos, 2);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    row_checkbox_color("aim tracer", &cfg.tracer, cfg.tracer_color, "aim_tracer_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("distance check", &cfg.distance_check);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    if (Visuals::HavocWorldEsp::IsActivePlace())
                    {
                        // aim в студax, но кап = 400m
                        float hi = Visuals::HavocWorldEsp::MaxRangeStuds();
                        float d = cfg.max_distance;
                        if (d < 50.f) d = 50.f;
                        if (d > hi) d = hi;
                        cfg.max_distance = d;
                        widgets::slider_float("max distance (400m cap)", &cfg.max_distance,
                                              50.0f, hi, "%.0f");
                    }

                    else
                    {
                        widgets::slider_float("max distance", &cfg.max_distance, 50.0f, 5000.0f, "%.0f");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("visible only", &cfg.visible_only);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("dead check", &cfg.dead_check);

                    if (Visuals::HavocWorldEsp::IsActivePlace()) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("target bots", &g_Settings.aim.target_bots);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("humanize", &cfg.humanize);

                    if (cfg.humanize) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("reaction", &cfg.reaction_ms, 0.0f, 400.0f, "%.0fms");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("sticky target", &cfg.sticky);

                    if (cfg.sticky) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f, "%.1fx");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("prediction", &cfg.prediction);

                    if (cfg.prediction) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("bullet speed", &cfg.bullet_speed,
                                              100.0f, 5000.0f, "%.0f");
                    }
                }
                widgets::end_child_panel();

                if (draw_side_child("aim_combat", "combat", aim_right_pos, aim_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    {
                        static const char* k_target[] = {
                            "fov center", "distance", "lowest hp"
                        };
                        widgets::combo("target select", &cfg.target_select, k_target, 3);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hitchance", &cfg.hitchance_enabled);

                    if (cfg.hitchance_enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("hit chance", &cfg.hitchance, 1.0f, 100.0f, "%.0f%%");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("hitbox expander", &g_Settings.hitbox.enabled);

                    if (g_Settings.hitbox.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* hb_parts[] = {
                                "head", "hrp", "torso", "arms", "legs"
                            };
                            widgets::combo("hb part", &g_Settings.hitbox.part, hb_parts,
                                           Settings::HB_PART_COUNT);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("hb scale", &g_Settings.hitbox.scale,
                                              1.0f, 20.0f, "%.1f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("hb visualize", &g_Settings.hitbox.visualize);

                        if (g_Settings.hitbox.visualize) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            static const char* hb_viz[] = { "2d", "3d", "3d filled" };
                            widgets::combo("hb viz mode", &g_Settings.hitbox.viz_mode, hb_viz, 3);
                            label_color_row("hb color", g_Settings.hitbox.viz_color, "hitbox_viz_color");
                        }
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("kill effects", &g_Settings.killfx.enabled);

                    if (g_Settings.killfx.enabled) {
                        if (g_Settings.killfx.effect < 0 ||
                            g_Settings.killfx.effect >= Visuals::KillEffects::EffectNameCount())
                            g_Settings.killfx.effect = 0;

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::combo("kill fx", &g_Settings.killfx.effect,
                                       Visuals::KillEffects::EffectNames(),
                                       Visuals::KillEffects::EffectNameCount());
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hitmarkers", &g_Settings.hitmarker.enabled);

                    if (g_Settings.hitmarker.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("marker size", &g_Settings.hitmarker.size, 0.5f, 2.5f, "%.2f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("marker time", &g_Settings.hitmarker.duration, 0.2f, 1.5f, "%.2fs");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hitsounds", &g_Settings.hitsound.enabled);

                    if (g_Settings.hitsound.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        if (g_Settings.hitsound.index < 0 ||
                            g_Settings.hitsound.index >= Features::HitSounds::Count())
                            g_Settings.hitsound.index = 0;
                        if (widgets::combo("hitsound", &g_Settings.hitsound.index,
                                           Features::HitSounds::Names(),
                                           Features::HitSounds::Count())) {
                            Features::HitSounds::Play(g_Settings.hitsound.index);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("hitsound volume", &g_Settings.hitsound.volume,
                                              0.0f, 100.0f, "%.0f%%");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hit data", &g_Settings.hitdata.enabled);

                    if (g_Settings.hitdata.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::multi_combo("data lines", g_Settings.hitdata.modes,
                                             Visuals::KillEffects::HitDataModeNames(),
                                             Visuals::KillEffects::HitDataModeNameCount());

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("data size", &g_Settings.hitdata.size, 10.0f, 28.0f, "%.0f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("data time", &g_Settings.hitdata.duration, 0.4f, 2.5f, "%.2fs");
                    }
                }
                widgets::end_child_panel();
            }

            else if (active_tab == 4)
            {
                const ImVec2 misc_child_size = make_side_child_size(2);
                const ImVec2 misc_left_pos(child_margin, child_margin);
                const ImVec2 misc_right_pos(
                    misc_left_pos.x + misc_child_size.x + side_child_gap,
                    child_margin);

                if (draw_side_child("misc_world", "world", misc_left_pos, misc_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    widgets::checkbox("teamcheck", &g_Settings.misc.teamcheck);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("no shadow", &g_Settings.world.no_shadow);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("brightness", &g_Settings.world.brightness, 0.0f, 20.0f, "%.1f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    row_checkbox_color("fog", &g_Settings.world.fog,
                        g_Settings.world.fog_color, "world_fog_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::slider_float("fog start", &g_Settings.world.fog_start, 0.0f, 100000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fog end", &g_Settings.world.fog_end, 0.0f, 100000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("fps unlocker", &g_Settings.misc.fps_unlock);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_int("fps cap", &g_Settings.misc.fps_cap, 60, 1000, "%d");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("fov changer", &g_Settings.misc.fov);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fov value", &g_Settings.misc.fov_value, 10.0f, 120.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("crosshair", &g_Settings.crosshair.enabled);

                    if (g_Settings.crosshair.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("cross length", &g_Settings.crosshair.length,
                                              1.0f, 40.0f, "%.0f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("cross gap", &g_Settings.crosshair.gap,
                                              0.0f, 30.0f, "%.0f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("cross thick", &g_Settings.crosshair.thickness,
                                              1.0f, 6.0f, "%.1f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("cross spin", &g_Settings.crosshair.spin);

                        if (g_Settings.crosshair.spin) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            widgets::slider_float("spin speed", &g_Settings.crosshair.spin_speed,
                                                  -360.0f, 360.0f, "%.0f");
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("cross outline", &g_Settings.crosshair.outline);

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::checkbox("cross dot", &g_Settings.crosshair.dot);

                        if (g_Settings.crosshair.dot) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            widgets::slider_float("dot size", &g_Settings.crosshair.dot_size,
                                                  1.0f, 8.0f, "%.1f");
                        }

                        label_color_row("cross color", g_Settings.crosshair.color, "crosshair_color");
                        label_color_row("outline color", g_Settings.crosshair.outline_color,
                                        "crosshair_outline_color");
                    }
                }
                widgets::end_child_panel();

                if (draw_side_child("misc_local", "local", misc_right_pos, misc_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    widgets::checkbox("walkspeed", &g_Settings.misc.walkspeed);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::keybind("ws key", &g_Settings.misc.walkspeed_key, &g_Settings.misc.walkspeed_key_mode);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_ws_modes[] = { "position", "humanoid" };
                        widgets::combo("ws mode", &g_Settings.misc.walkspeed_mode, k_ws_modes, 2);
                    }
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("ws value", &g_Settings.misc.walkspeed_value, 1.0f, 500.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("jump power", &g_Settings.misc.jump);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("jump value", &g_Settings.misc.jump_power, 50.0f, 500.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("fly", &g_Settings.misc.fly);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::keybind("fly key", &g_Settings.misc.fly_key, &g_Settings.misc.fly_key_mode);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fly speed", &g_Settings.misc.fly_speed, 5.0f, 1000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::keybind("freecam", &g_Settings.misc.freecam_key, &g_Settings.misc.freecam_mode);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("freecam speed", &g_Settings.misc.freecam_speed, 10.0f, 300.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("freecam sens", &g_Settings.misc.freecam_sens, 0.05f, 1.0f, "%.2f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox_keybind(
                        "third person",
                        &g_Settings.misc.third_person,
                        &g_Settings.misc.third_person_key,
                        &g_Settings.misc.third_person_mode);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float(
                        "camera distance",
                        &g_Settings.misc.third_person_distance,
                        1.0f, 120.0f, "%.1f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("noclip", &g_Settings.misc.noclip);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("infinite jump", &g_Settings.misc.inf_jump);
                }
                widgets::end_child_panel();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    if (!m_bMenuVisible && !animation_running && window_alpha <= 0.0f) {
        return window_alpha;
    }

    float dock_gap = 6.f;
    float dock_x = s_menu_pos.x + s_menu_size.x;

    g_Settings.esp.preview = true;
    if ((active_tab == 0 || active_tab == 3) &&
        (m_bMenuVisible || animation_running || window_alpha > 0.0f))
    {
        float preview_w = 360.f;
        float preview_x = dock_x + dock_gap;

        ImGui::SetNextWindowPos(ImVec2(preview_x, s_menu_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(preview_w, s_menu_size.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(260.f, s_menu_size.y),
            ImVec2(640.f, s_menu_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        {
            ImVec4 grip = colors::child_fill; grip.w = 0.35f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGrip, grip);
            ImVec4 a = colors::accent; a.w = 0.7f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, a);
            a.w = 1.0f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, a);
        }

        const ImGuiWindowFlags k_preview_flags =
            ImGuiWindowFlags_NoTitleBar    |
            ImGuiWindowFlags_NoScrollbar   |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##jewsploit_esp_preview", nullptr, k_preview_flags))
        {

            const ImVec2 cur = ImGui::GetWindowSize();
            if (ImFabs(cur.y - s_menu_size.y) > 0.5f)
                ImGui::SetWindowSize(ImVec2(cur.x, s_menu_size.y));

            dock_x = preview_x + ImGui::GetWindowSize().x;

            colors::draw_panel_background(window_alpha);

            float margin = 10.f;
            ImVec2 win_sz = ImGui::GetWindowSize();
            float child_w = win_sz.x - margin * 2.f;
            float child_h = win_sz.y - margin * 2.f;

            float title_font_size = fonts::ui_size(fonts::ui_bold());

            ImGui::SetCursorPos(ImVec2(margin, margin));
            if (widgets::begin_child_panel(
                    "esp_preview_child",
                    ImVec2(child_w, child_h),
                    "esp preview",
                    fonts::ui_bold(),
                    title_font_size,
                    nullptr, nullptr, nullptr))
            {
                Cheat::Visuals::ESPPreview::Render();
            }
            widgets::end_child_panel();
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    if (active_tab == 4 && g_Settings.misc.custom_support &&
        (m_bMenuVisible || animation_running || window_alpha > 0.0f))
    {
        float cs_default_w = 340.f;
        float cs_x = dock_x + dock_gap;

        ImGui::SetNextWindowPos(ImVec2(cs_x, s_menu_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cs_default_w, s_menu_size.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(200.0f, s_menu_size.y),
            ImVec2(480.0f, s_menu_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        {
            ImVec4 grip = colors::child_fill; grip.w = 0.35f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGrip, grip);
            ImVec4 a = colors::accent; a.w = 0.7f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, a);
            a.w = 1.0f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, a);
        }

        const ImGuiWindowFlags k_cs_flags =
            ImGuiWindowFlags_NoTitleBar    |
            ImGuiWindowFlags_NoScrollbar   |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##jewsploit_custom", nullptr, k_cs_flags))
        {
            const ImVec2 cur = ImGui::GetWindowSize();
            if (ImFabs(cur.y - s_menu_size.y) > 0.5f)
                ImGui::SetWindowSize(ImVec2(cur.x, s_menu_size.y));
            dock_x = cs_x + ImGui::GetWindowSize().x;

            colors::draw_panel_background(window_alpha);

            float margin = 10.f;
            ImVec2 win_sz = ImGui::GetWindowSize();
            float child_w = win_sz.x - margin * 2.f;
            float child_h = win_sz.y - margin * 2.f;
            float tf = fonts::ui_size(fonts::ui_bold());

            ImGui::SetCursorPos(ImVec2(margin, margin));
            if (widgets::begin_child_panel(
                    "custom_child", ImVec2(child_w, child_h),
                    "custom support", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
            {
                static char s_label[64] = "";
                ImGui::SetCursorPosX(6.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                const bool submit = widgets::input_text(
                    "##cs_label", "target name", s_label, IM_ARRAYSIZE(s_label),
                    0.0f, ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::SetCursorPosX(6.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                if ((widgets::button("add target", ImVec2(90.0f, 0.0f)) || submit)
                    && s_label[0] != '\0') {
                    Cheat::CustomTarget t{};
                    ImFormatString(t.label, IM_ARRAYSIZE(t.label), "%s", s_label);
                    Cheat::g_CustomTargets.push_back(t);
                    s_label[0] = '\0';
                }

                ImGui::SetCursorPosX(4.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                const float list_w = ImGui::GetContentRegionAvail().x - 2.0f;
                const float list_h = ImMax(120.0f, ImGui::GetContentRegionAvail().y - 4.0f);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::BeginChild("cs_scroll", ImVec2(list_w, list_h), false))
                {
                    int remove = -1;
                    for (std::size_t i = 0; i < Cheat::g_CustomTargets.size(); ++i) {
                        Cheat::CustomTarget& t = Cheat::g_CustomTargets[i];
                        ImGui::PushID(static_cast<int>(i));

                        const float blk_w = ImGui::GetContentRegionAvail().x - 2.0f;
                        ImGui::SetCursorPosX(2.0f);
                        if (widgets::begin_child_panel(
                                "cs_blk", ImVec2(blk_w, 270.0f),
                                t.label[0] ? t.label : "target",
                                fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                            widgets::checkbox("enabled", &t.enabled);
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(blk_w - 24.0f);
                            if (widgets::button("x", ImVec2(16.0f, 0.0f)))
                                remove = static_cast<int>(i);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            {
                                static const char* k_kind[] = { "folder", "model" };
                                widgets::combo("type", &t.kind, k_kind, 2);
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            {
                                static const char* k_resolve[] = { "exact path", "by name" };
                                widgets::combo("resolve", &t.resolve, k_resolve, 2);
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                            widgets::input_text("##cs_q",
                                t.resolve == 0 ? "paste path / address" : "paste name",
                                t.query, IM_ARRAYSIZE(t.query), 0.0f, 0);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                            const float vis_w = ImGui::GetContentRegionAvail().x - 6.0f;
                            if (widgets::begin_child_panel(
                                    "cs_vis", ImVec2(vis_w, 118.0f),
                                    "visuals", fonts::ui_bold(), tf, nullptr, nullptr, nullptr))
                            {
                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                                row_checkbox_color("box", &t.vis.box, t.vis.box_color, "cs_box");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("filled", &t.vis.filled, t.vis.fill_color, "cs_fill");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("name", &t.vis.name, t.vis.name_color, "cs_name");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("distance", &t.vis.distance, t.vis.distance_color, "cs_dist");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("tracer", &t.vis.tracer, t.vis.tracer_color, "cs_tracer");
                            }
                            widgets::end_child_panel();
                        }
                        widgets::end_child_panel();

                        ImGui::PopID();
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                    }

                    if (remove >= 0)
                        Cheat::g_CustomTargets.erase(Cheat::g_CustomTargets.begin() + remove);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            widgets::end_child_panel();
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    return window_alpha;
}

// гасим хуки сайлента + imgui
void Menu::Shutdown()
{
    if (!m_bInitialized)
        return;

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
