#include "pch.h"
#define NOMINMAX
#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui_internal.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#undef GetClassName

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace {
    ImVec2 g_wm_size{};
    bool g_wm_dragging = false;

    ImU32 with_alpha(ImU32 color, float alpha)
    {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        float a = alpha;
        if (a < 0.f) a = 0.f;
        if (a > 1.f) a = 1.f;
        value.w *= a;
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    float measure_text(ImFont* font, float font_size, const char* text) {
        if (text == nullptr || text[0] == '\0')
            return 0.0f;
        if (font != nullptr)
            return font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
        return ImGui::CalcTextSize(text).x;
    }

    // имя локала из Players
    std::string LocalPlayerName() {
        if (!Cheat::Globals::Players || !Cheat::Globals::Players->address)
            return {};
        const auto lp = g_Memory.Read<std::uint64_t>(
            Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
        if (!lp || !g_Memory.IsValid(lp))
            return {};
        Cheat::Player pl(lp);
        std::string dn = pl.GetDisplayName();
        if (!dn.empty() && dn != "Unknown")
            return dn;
        return pl.GetName();
    }
}

namespace widgets {
    bool watermark_hit_test(float x, float y) {
        if (!Cheat::g_Settings.gui.watermark)
            return false;
        if (g_wm_dragging)
            return true;
        if (g_wm_size.x <= 0.0f || g_wm_size.y <= 0.0f)
            return false;
        const float wx = Cheat::g_Settings.gui.watermark_x;
        const float wy = Cheat::g_Settings.gui.watermark_y;
        return x >= wx && y >= wy && x < wx + g_wm_size.x && y < wy + g_wm_size.y;
    }

    void watermark(float alpha)
    {
        ImVec2 pad(8.f, 5.f);
        float seg_gap = 8.f;

        if (alpha < 0.f) alpha = 0.f;
        if (alpha > 1.f) alpha = 1.f;
        if (alpha <= 0.f)
        {
            g_wm_dragging = false;
            return;
        }

        auto& gui = Cheat::g_Settings.gui;
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();

        ImFont* brand_font = fonts::ui_bold();
        ImFont* meta_font = fonts::ui();
        const float brand_font_size = fonts::ui_size(brand_font);
        const float meta_font_size = fonts::ui_size(meta_font);

        const widgets::text_span brand_spans[] = {
            {"jew",    with_alpha(colors::accent_u32(), alpha)},
            {"sploit", with_alpha(colors::text_active_u32(), alpha)},
        };

        std::vector<std::string> segments;
        segments.reserve(Cheat::Settings::WM_FIELD_COUNT);

        if (gui.watermark_fields[Cheat::Settings::WM_BUILD]) {
            char buf[64];
            ImFormatString(buf, IM_ARRAYSIZE(buf), "build: %s", __DATE__);
            segments.emplace_back(buf);
        }
        if (gui.watermark_fields[Cheat::Settings::WM_PLAYER]) {
            const std::string name = LocalPlayerName();
            segments.emplace_back(name.empty() ? "player: -" : ("user: " + name));
        }
        if (gui.watermark_fields[Cheat::Settings::WM_PLACE_ID]) {
            char buf[64];
            const auto place = Cheat::Globals::InstanceDataModel.address
                ? Cheat::Globals::InstanceDataModel.GetPlaceId() : 0ull;
            ImFormatString(buf, IM_ARRAYSIZE(buf), "place: %llu",
                           static_cast<unsigned long long>(place));
            segments.emplace_back(buf);
        }
        if (gui.watermark_fields[Cheat::Settings::WM_GAME_ID]) {
            char buf[64];
            const auto game = Cheat::Globals::InstanceDataModel.address
                ? Cheat::Globals::InstanceDataModel.GetGameId() : 0ull;
            ImFormatString(buf, IM_ARRAYSIZE(buf), "game: %llu",
                           static_cast<unsigned long long>(game));
            segments.emplace_back(buf);
        }
        if (gui.watermark_fields[Cheat::Settings::WM_TIME]) {
            char buf[32];
            const std::time_t now = std::time(nullptr);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &now);
#else
            local = *std::localtime(&now);
#endif
            std::strftime(buf, sizeof(buf), "%H:%M:%S", &local);
            segments.emplace_back(std::string("time: ") + buf);
        }
        if (gui.watermark_fields[Cheat::Settings::WM_FPS]) {
            char buf[32];
            ImFormatString(buf, IM_ARRAYSIZE(buf), "fps: %.0f", io.Framerate);
            segments.emplace_back(buf);
        }

        const float brand_jew_w = measure_text(brand_font, brand_font_size, brand_spans[0].text);
        const float brand_sploit_w = measure_text(brand_font, brand_font_size, brand_spans[1].text);
        const float brand_w = brand_jew_w + brand_sploit_w;

        const float text_h = brand_font->CalcTextSizeA(brand_font_size, FLT_MAX, 0.0f, "A").y;
        const float meta_text_h = meta_font->CalcTextSizeA(meta_font_size, FLT_MAX, 0.0f, "A").y;

        float content_w = brand_w;
        for (const auto& seg : segments) {
            content_w += seg_gap + measure_text(meta_font, meta_font_size, seg.c_str());
        }

        const float total_w = pad.x * 2.0f + content_w;
        const float total_h = pad.y * 2.0f + text_h;
        g_wm_size = ImVec2(total_w, total_h);

        float max_x = io.DisplaySize.x - total_w;
        float max_y = io.DisplaySize.y - total_h;
        if (max_x < 0.f) max_x = 0.f;
        if (max_y < 0.f) max_y = 0.f;
        if (gui.watermark_x < 0.f) gui.watermark_x = 0.f;
        if (gui.watermark_x > max_x) gui.watermark_x = max_x;
        if (gui.watermark_y < 0.f) gui.watermark_y = 0.f;
        if (gui.watermark_y > max_y) gui.watermark_y = max_y;

        // абсолютный grab без лагов/джиттера
        {
            static ImVec2 s_grab{};

            ImGui::SetNextWindowPos(ImVec2(gui.watermark_x, gui.watermark_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(total_w, total_h), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            if (ImGui::Begin(
                    "##jewsploit_watermark_drag",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoBackground))
            {
                ImGui::InvisibleButton("##wm_drag", ImVec2(total_w, total_h));

                if (ImGui::IsItemActivated()) {
                    g_wm_dragging = true;
                    s_grab = io.MousePos - ImVec2(gui.watermark_x, gui.watermark_y);
                }

                if (g_wm_dragging)
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        gui.watermark_x = io.MousePos.x - s_grab.x;
                        gui.watermark_y = io.MousePos.y - s_grab.y;
                        if (gui.watermark_x < 0.f) gui.watermark_x = 0.f;
                        if (gui.watermark_x > max_x) gui.watermark_x = max_x;
                        if (gui.watermark_y < 0.f) gui.watermark_y = 0.f;
                        if (gui.watermark_y > max_y) gui.watermark_y = max_y;
                        ImGui::SetWindowPos(ImVec2(gui.watermark_x, gui.watermark_y), ImGuiCond_Always);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetNextFrameWantCaptureMouse(true);
                    }

                    else
                    {
                        g_wm_dragging = false;
                    }
                }

                else if (ImGui::IsItemHovered())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(3);
        }

        const float intro_slide = (1.0f - alpha) * 12.0f;
        const ImVec2 origin(gui.watermark_x - intro_slide, gui.watermark_y);
        const ImVec2 rect_max(origin.x + total_w, origin.y + total_h);

        const ImU32 fill_top = with_alpha(ImGui::ColorConvertFloat4ToU32(colors::panel_fill), alpha);
        const ImU32 fill_bot = with_alpha(ImGui::ColorConvertFloat4ToU32(colors::child_fill), alpha);
        draw_list->AddRectFilledMultiColor(origin, rect_max, fill_top, fill_top, fill_bot, fill_bot);
        draw_list->AddRect(origin, rect_max, with_alpha(IM_COL32(0, 0, 0, 255), alpha), 0.0f, 0, 1.0f);
        draw_list->AddRect(
            ImVec2(origin.x + 1.0f, origin.y + 1.0f),
            ImVec2(rect_max.x - 1.0f, rect_max.y - 1.0f),
            with_alpha(ImGui::ColorConvertFloat4ToU32(colors::inner_border), alpha),
            0.0f,
            0,
            1.0f);

        const float brand_text_y = ImFloor(origin.y + pad.y);
        const float meta_text_y = ImFloor(origin.y + pad.y + (text_h - meta_text_h) * 0.5f);
        float cursor_x = origin.x + pad.x;

        const float phase = static_cast<float>(ImGui::GetTime()) * 2.4f;
        const float wavelength = 70.0f;
        const ImU32 highlight = with_alpha(
            ImGui::ColorConvertFloat4ToU32(ImLerp(colors::text_active, colors::accent, 0.35f)),
            alpha);

        widgets::draw_outlined_text_spans_shimmer(
            draw_list,
            brand_font,
            brand_font_size,
            ImVec2(ImFloor(cursor_x), brand_text_y),
            brand_spans,
            IM_ARRAYSIZE(brand_spans),
            highlight,
            phase,
            wavelength);
        cursor_x += brand_w;

        for (const auto& seg : segments) {
            cursor_x += seg_gap;
            draw_outlined_text(
                draw_list,
                meta_font,
                meta_font_size,
                ImVec2(ImFloor(cursor_x), meta_text_y),
                with_alpha(colors::text_inactive_u32(), alpha),
                seg.c_str());
            cursor_x += measure_text(meta_font, meta_font_size, seg.c_str());
        }
    }
}
