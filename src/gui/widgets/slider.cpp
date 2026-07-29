#include "pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui_internal.h"
#include <cstdio>

namespace {
    //constexpr float k_slider_width_default = 200.0f;
    //constexpr float k_slider_width_min = 100.0f;
    //constexpr float k_widget_side_pad = 8.0f;
    //constexpr float k_slider_height = 20.0f;
    //constexpr float k_label_track_gap = 4.0f;
    //constexpr float k_label_text_offset_x = 1.0f;
    //constexpr float k_value_track_pad = 5.0f;
    //constexpr int k_fill_gradient_count = 8;

    float slider_width()
    {
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.f)
            return 200.f;
        float w = avail - 8.f; // side pad
        if (w < 100.f) w = 100.f;
        return w;
    }

    void draw_vertical_gradient_fill(ImDrawList* dl, const ImRect& fill_rect)
    {
        int fl = (int)ImFloor(fill_rect.Min.x);
        int ft = (int)ImFloor(fill_rect.Min.y);
        int fr = (int)ImCeil(fill_rect.Max.x) - 1;
        int fb = (int)ImCeil(fill_rect.Max.y) - 1;
        int fh = fb - ft + 1;
        if (fh <= 0)
            return;

        // 8 строк градиента хватит
        int rows = fh;
        if (rows > 8) rows = 8;
        for (int i = 0; i < rows; ++i)
        {
            dl->AddRectFilled(
                ImVec2((float)fl, (float)(ft + i)),
                ImVec2((float)(fr + 1), (float)(ft + i + 1)),
                colors::accent_gradient_row(i, 8));
        }

        if (rows < fh)
        {
            // хвост добиваем последним цветом градиента
            dl->AddRectFilled(
                ImVec2((float)fl, (float)(ft + rows)),
                ImVec2((float)(fr + 1), (float)(fb + 1)),
                colors::accent_gradient_row(7, 8));
        }
    }

    void draw_slider_track(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float fill_t)
    {
        ImRect outer(min, max);
        ImRect inner(
            ImVec2(outer.Min.x + 1.f, outer.Min.y + 1.f),
            ImVec2(outer.Max.x - 1.f, outer.Max.y - 1.f));
        ImRect fill(
            ImVec2(inner.Min.x + 1.f, inner.Min.y + 1.f),
            ImVec2(inner.Max.x - 1.f, inner.Max.y - 1.f));

        dl->AddRectFilled(fill.Min, fill.Max, colors::widget_track_u32());

        float t = fill_t;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        if (t > 0.f)
        {
            ImRect filled(
                fill.Min,
                ImVec2(fill.Min.x + t * (fill.Max.x - fill.Min.x), fill.Max.y));
            draw_vertical_gradient_fill(dl, filled);
        }

        dl->AddRect(inner.Min, inner.Max, colors::widget_inline_u32(), 0.f, 0, 1.f);
        dl->AddRect(outer.Min, outer.Max, colors::widget_outline_u32(), 0.f, 0, 1.f);
    }

    float normalized_value(ImGuiDataType type, void* data, const void* v_min, const void* v_max)
    {
        if (type == ImGuiDataType_Float)
        {
            float vmin = *(const float*)v_min;
            float vmax = *(const float*)v_max;
            float range = vmax - vmin;
            if (range <= 0.f)
                return 0.f;
            float t = (*(float*)data - vmin) / range;
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            return t;
        }

        int vmin = *(const int*)v_min;
        int vmax = *(const int*)v_max;
        int range = vmax - vmin;
        if (range <= 0)
            return 0.f;
        float t = (float)(*(int*)data - vmin) / (float)range;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        return t;
    }

    bool slider_scalar(
        const char* label,
        ImGuiDataType data_type,
        void* data,
        const void* v_min,
        const void* v_max,
        const char* format)
    {
        if (!data)
            return false;

        widgets::menu_row();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label ? label : "";
        if (!format)
            format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

        float slider_h = 20.f;
        float label_gap = 4.f;
        float value_pad = 5.f;

        ImGuiID id = window->GetID(text);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImFont* font = fonts::ui();
        float fs = fonts::ui_size(font);
        ImVec2 label_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);

        char value_buf[64];
        ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, data, format);

        float value_fs = fs - 2.f;
        if (value_fs < 10.f) value_fs = 10.f;
        float row_h = label_sz.y;
        float total_h = slider_h;
        if (text[0] != '\0')
            total_h = row_h + label_gap + slider_h;
        float track_w = slider_width();
        ImVec2 total_sz(track_w, total_h);

        ImVec2 track_min(pos.x, pos.y);
        if (text[0] != '\0')
            track_min.y = pos.y + row_h + label_gap;
        ImVec2 track_max(track_min.x + track_w, track_min.y + slider_h);
        ImRect track_bb(track_min, track_max);
        ImRect total_bb(pos, pos + total_sz);

        ImGui::ItemSize(total_sz);
        if (!ImGui::ItemAdd(total_bb, id, &track_bb))
            return false;

        bool hovered = ImGui::ItemHoverable(track_bb, id, g.LastItemData.ItemFlags);
        bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, ImGuiInputFlags_None, id);
        bool make_active = clicked || g.NavActivateId == id;

        if (make_active)
        {
            if (clicked)
                ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }

        ImRect grab_bb;
        bool changed = ImGui::SliderBehavior(
            track_bb, id, data_type, data, v_min, v_max, format,
            ImGuiSliderFlags_None, &grab_bb);

        if (changed)
        {
            ImGui::MarkItemEdited(id);
            ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, data, format);
        }

        float rel = normalized_value(data_type, data, v_min, v_max);
        bool held = g.ActiveId == id;

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.f);
        *anim_hover = ImLerp(*anim_hover, (hovered || held) ? 1.f : 0.f, 15.f * g.IO.DeltaTime);

        float* anim_value = window->StateStorage.GetFloatRef(id + 1, rel);
        *anim_value = ImLerp(*anim_value, rel, 15.f * g.IO.DeltaTime);

        if (text[0] != '\0')
        {
            widgets::draw_outlined_text(
                dl, font, fs,
                ImVec2(ImFloor(pos.x + 1.f), ImFloor(pos.y)),
                colors::label_u32(*anim_hover), text);
        }

        draw_slider_track(dl, track_min, track_max, *anim_value);

        ImVec2 val_sz = font->CalcTextSizeA(value_fs, FLT_MAX, 0.f, value_buf);
        float fill_x = track_min.x + *anim_value * (track_max.x - track_min.x);
        float value_x = fill_x - val_sz.x * 0.5f;
        float lo = track_min.x + value_pad;
        float hi = track_max.x - val_sz.x - value_pad;
        if (value_x < lo) value_x = lo;
        if (value_x > hi) value_x = hi;
        float value_y = track_min.y + (slider_h - val_sz.y) * 0.5f;

        widgets::draw_outlined_text(
            dl, font, value_fs,
            ImVec2(ImFloor(value_x), ImFloor(value_y)),
            colors::text_active_u32(), value_buf);

        return changed;
    }
}

namespace widgets {
    bool slider_float(const char* label, float* value, float v_min, float v_max, const char* format) {
        if (!value)
            return false;
        if (!format)
            format = "%.3f";
        return slider_scalar(label, ImGuiDataType_Float, value, &v_min, &v_max, format);
    }

    bool slider_int(const char* label, int* value, int v_min, int v_max, const char* format) {
        if (!value)
            return false;
        if (!format)
            format = "%d";
        return slider_scalar(label, ImGuiDataType_S32, value, &v_min, &v_max, format);
    }
}
