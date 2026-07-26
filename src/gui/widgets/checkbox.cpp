#include "pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui_internal.h"

namespace {
    //constexpr float k_box_size = 14.0f;
    //constexpr float k_label_spacing = 8.0f;
    //constexpr float k_box_offset_y = 7.0f;
    //constexpr int k_checked_gradient_rows = 8;

    ImU32 lerp_color(ImU32 from, ImU32 to, float t)
    {
        return ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImGui::ColorConvertU32ToFloat4(from),
            ImGui::ColorConvertU32ToFloat4(to),
            t));
    }

    void draw_checkbox_fill(ImDrawList* dl, const ImRect& fill_rect, float check_t, bool hovered)
    {
        int fl = (int)ImFloor(fill_rect.Min.x);
        int ft = (int)ImFloor(fill_rect.Min.y);
        int fr = (int)ImCeil(fill_rect.Max.x) - 1;
        int fb = (int)ImCeil(fill_rect.Max.y) - 1;
        int fh = fb - ft + 1;
        if (fh <= 0)
            return;

        float t = check_t;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;

        ImU32 base = colors::widget_track_u32();
        if (hovered && t < 1.f)
            base = colors::widget_track_hover_u32();

        // построчный градиент когда чекнуто а то тупо плоско
        for (int i = 0; i < fh; ++i)
        {
            int gi = i;
            if (gi > 7) gi = 7; // 8 rows
            ImU32 checked = colors::accent_gradient_row(gi, 8);
            ImU32 row = lerp_color(base, checked, t);
            dl->AddRectFilled(
                ImVec2((float)fl, (float)(ft + i)),
                ImVec2((float)(fr + 1), (float)(ft + i + 1)),
                row);
        }
    }

    void draw_checkbox_box(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float check_t, bool hovered)
    {
        ImRect outer(min, max);
        ImRect inner(
            ImVec2(outer.Min.x + 1.f, outer.Min.y + 1.f),
            ImVec2(outer.Max.x - 1.f, outer.Max.y - 1.f));
        ImRect fill(
            ImVec2(inner.Min.x + 1.f, inner.Min.y + 1.f),
            ImVec2(inner.Max.x - 1.f, inner.Max.y - 1.f));

        draw_checkbox_fill(dl, fill, check_t, hovered);
        dl->AddRect(inner.Min, inner.Max, colors::widget_inline_u32(), 0.f, 0, 1.f);
        dl->AddRect(outer.Min, outer.Max, colors::widget_outline_u32(), 0.f, 0, 1.f);
    }
}

namespace widgets {
    bool checkbox(const char* label, bool* value)
    {
        if (!value)
            return false;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label ? label : "";
        ImGuiID id = window->GetID(text);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImFont* font = fonts::ui();
        float fs = fonts::ui_size(font);

        float box = 14.f;
        float label_gap = 8.f;
        float box_oy = 7.f;

        ImVec2 label_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
        float total_w = box;
        if (text[0] != '\0')
            total_w = box + label_gap + label_sz.x;
        ImVec2 total_sz(total_w, box + box_oy);
        ImRect bounds(pos, pos + total_sz);

        ImGui::ItemSize(total_sz);
        if (!ImGui::ItemAdd(bounds, id))
            return false;

        bool hovered = false;
        bool held = false;
        bool pressed = ImGui::ButtonBehavior(bounds, id, &hovered, &held);
        if (pressed)
            *value = !*value;

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.f);
        *anim_hover = ImLerp(*anim_hover, (hovered || *value) ? 1.f : 0.f, 15.f * g.IO.DeltaTime);

        float* anim_check = window->StateStorage.GetFloatRef(id + 1, 0.f);
        *anim_check = ImLerp(*anim_check, *value ? 1.f : 0.f, 15.f * g.IO.DeltaTime);

        ImVec2 box_min(pos.x, pos.y + box_oy);
        ImVec2 box_max(box_min.x + box, box_min.y + box);
        draw_checkbox_box(dl, box_min, box_max, *anim_check, hovered);

        if (text[0] != '\0')
        {
            ImVec2 text_pos(
                ImFloor(box_max.x + label_gap),
                ImFloor(box_min.y + (box - label_sz.y) * 0.5f));
            draw_outlined_text(dl, font, fs, text_pos, colors::label_u32(*anim_hover), text);
        }

        return pressed;
    }
}
