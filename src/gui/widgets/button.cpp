#include "pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui_internal.h"

namespace {
    //constexpr float k_button_height = 20.0f;
    //constexpr float k_text_pad_x = 12.0f;
    //constexpr float k_input_pad_x = 6.0f;
    //constexpr float k_input_pad_y = 3.0f;

    ImFont* label_font()
    {
        return fonts::ui();
    }

    float label_font_size()
    {
        return fonts::ui_size();
    }

    // рамка кнопок/инпута outer/inline/fill
    void draw_framed_box(ImDrawList* dl, const ImVec2& min, const ImVec2& max)
    {
        ImRect outer(min, max);
        ImRect inner(
            ImVec2(outer.Min.x + 1.f, outer.Min.y + 1.f),
            ImVec2(outer.Max.x - 1.f, outer.Max.y - 1.f));
        ImRect fill(
            ImVec2(inner.Min.x + 1.f, inner.Min.y + 1.f),
            ImVec2(inner.Max.x - 1.f, inner.Max.y - 1.f));

        dl->AddRectFilled(fill.Min, fill.Max, colors::widget_track_u32());
        dl->AddRect(inner.Min, inner.Max, colors::widget_inline_u32(), 0.f, 0, 1.f);
        dl->AddRect(outer.Min, outer.Max, colors::widget_outline_u32(), 0.f, 0, 1.f);
    }
}

namespace widgets {
    bool button(const char* label, const ImVec2& size_arg)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label ? label : "";

        ImFont* font = label_font();
        float fs = label_font_size();
        ImVec2 text_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);

        float w = size_arg.x > 0.f ? size_arg.x : text_sz.x + 12.f * 2.f;
        float h = size_arg.y > 0.f ? size_arg.y : 20.f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 size(w, h);
        ImRect bb(pos, pos + size);

        ImGui::ItemSize(size);
        ImGuiID id = window->GetID(text);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered = false;
        bool held = false;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        float* anim = window->StateStorage.GetFloatRef(id, 0.f);
        *anim = ImLerp(*anim, hovered ? 1.f : 0.f, 15.f * g.IO.DeltaTime);

        draw_framed_box(dl, pos, pos + size);

        if (*anim > 0.01f)
        {
            ImRect hover_fill(
                ImVec2(pos.x + 2.f, pos.y + 2.f),
                ImVec2(pos.x + w - 2.f, pos.y + h - 2.f));
            dl->AddRectFilled(
                hover_fill.Min, hover_fill.Max,
                colors::widget_track_hover_u32(*anim));
        }

        ImVec2 text_pos(
            ImFloor(pos.x + (w - text_sz.x) * 0.5f),
            ImFloor(pos.y + (h - text_sz.y) * 0.5f));
        draw_outlined_text(dl, font, fs, text_pos, colors::label_u32(*anim), text);

        return pressed;
    }

    bool icon_close(const char* id, const ImVec2& size_arg)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* sid = id ? id : "##close";
        float w = size_arg.x > 0.f ? size_arg.x : 14.f;
        float h = size_arg.y > 0.f ? size_arg.y : 14.f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 size(w, h);
        ImRect bb(pos, pos + size);

        ImGui::ItemSize(size);
        ImGuiID wid = window->GetID(sid);
        if (!ImGui::ItemAdd(bb, wid))
            return false;

        bool hovered = false;
        bool held = false;
        bool pressed = ImGui::ButtonBehavior(bb, wid, &hovered, &held);

        float* anim = window->StateStorage.GetFloatRef(wid, 0.f);
        *anim = ImLerp(*anim, hovered ? 1.f : 0.f, 15.f * g.IO.DeltaTime);

        draw_framed_box(dl, pos, pos + size);
        if (*anim > 0.01f)
        {
            ImRect hover_fill(
                ImVec2(pos.x + 2.f, pos.y + 2.f),
                ImVec2(pos.x + w - 2.f, pos.y + h - 2.f));
            dl->AddRectFilled(
                hover_fill.Min, hover_fill.Max,
                colors::widget_track_hover_u32(*anim));
        }

        const float inset = ImMax(3.f, ImFloor(ImMin(w, h) * 0.28f));
        const ImU32 col = colors::label_u32(*anim);
        const float thick = 1.25f;
        dl->AddLine(
            ImVec2(pos.x + inset, pos.y + inset),
            ImVec2(pos.x + w - inset, pos.y + h - inset),
            col, thick);
        dl->AddLine(
            ImVec2(pos.x + w - inset, pos.y + inset),
            ImVec2(pos.x + inset, pos.y + h - inset),
            col, thick);

        return pressed;
    }

    bool input_text(const char* id, const char* hint, char* buffer, int buffer_size,
                    float width, ImGuiInputTextFlags flags)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems || !buffer)
            return false;

        ImGuiContext& g = *GImGui;
        if (width <= 0.f)
            width = ImMax(60.f, ImGui::GetContentRegionAvail().x);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(width, 20.f); // button h
        ImDrawList* dl = ImGui::GetWindowDrawList();
        draw_framed_box(dl, pos, pos + size);

        // клик по всей рамке должен фокусить поле
        float pad_y = ImMax(1.f, ImFloor((20.f - g.FontSize) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, colors::text_active);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, colors::text_inactive);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, pad_y));

        ImGui::SetCursorScreenPos(pos);
        bool result = ImGui::InputTextEx(
            id, hint, buffer, buffer_size, size, flags);

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(6);
        return result;
    }
}
