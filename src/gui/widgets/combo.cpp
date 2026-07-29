#include "pch.h"
#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui_internal.h"
#include <cstdio>

namespace {
    // ширина комбы от avail
    float combo_width()
    {
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.f)
            return 200.f;
        float w = avail - 8.f;
        if (w < 100.f) w = 100.f;
        return w;
    }

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
        dl->AddRect(outer.Min, outer.Max, IM_COL32(0, 0, 0, 255), 0.f, 0, 1.f);
    }
}

namespace widgets {
    bool combo(const char* label, int* current_item, const char* const items[], int items_count)
    {
        float combo_h = 20.f;
        float label_gap = 4.f;
        float label_ox = 1.f;
        float item_h = 20.f;
        float text_pad = 6.f;
        float marker_gap = 3.f;

        if (!current_item || !items || items_count <= 0)
            return false;

        menu_row();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label ? label : "";
        const ImGuiID id = window->GetID(text);

        if (*current_item < 0)
            *current_item = 0;
        if (*current_item >= items_count)
            *current_item = items_count - 1;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImVec2 label_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text);

        const float row_h = label_sz.y;
        const float bottom_gap = 4.f; // воздух под дропбоксом до следующего ряда
        const float total_h = row_h + label_gap + combo_h + bottom_gap;
        const float box_w = combo_width();
        const ImVec2 total_sz(box_w, total_h);

        const ImVec2 box_min(pos.x, pos.y + row_h + label_gap);
        const ImVec2 box_max(box_min.x + box_w, box_min.y + combo_h);
        const ImRect box_bb(box_min, box_max);
        const ImRect total_bb(pos, pos + total_sz);

        ImGui::ItemSize(total_sz);
        if (!ImGui::ItemAdd(total_bb, id, &box_bb))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);

        char popup_id[128];
        ImFormatString(popup_id, IM_ARRAYSIZE(popup_id), "%s##popup", text);
        if (pressed)
            ImGui::OpenPopup(popup_id);

        float* anim = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim = ImLerp(*anim, (hovered || ImGui::IsPopupOpen(popup_id)) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        const ImU32 label_col = colors::label_u32(*anim);

        if (text[0] != '\0')
        {
            draw_outlined_text(
                dl, font, fs,
                ImVec2(ImFloor(pos.x + label_ox), ImFloor(pos.y)),
                label_col, text);
        }

        draw_framed_box(dl, box_min, box_max);

        const char* preview = items[*current_item] ? items[*current_item] : "";
        const ImVec2 preview_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, preview);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_min.x + text_pad),
                ImFloor(box_min.y + (combo_h - preview_sz.y) * 0.5f)),
            label_col, preview);

        const bool is_open = ImGui::IsPopupOpen(popup_id);
        const char* icon = is_open ? "-" : "+";
        const ImVec2 icon_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, icon);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_max.x - icon_sz.x - text_pad),
                ImFloor(box_min.y + (combo_h - icon_sz.y) * 0.5f)),
            colors::accent_u32(), icon);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        bool value_changed = false;

        float popup_max_rows = 12.f;
        float content_h = items_count * item_h;
        float popup_h = item_h * popup_max_rows;
        if (content_h < popup_h) popup_h = content_h;
        bool need_scroll = content_h > popup_h + 0.5f;

        ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 1.0f));
        ImGui::SetNextWindowSize(ImVec2(box_w, popup_h));
        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        {
            ImDrawList* popup_dl = ImGui::GetWindowDrawList();
            const ImVec2 popup_min = ImGui::GetWindowPos();
            const ImVec2 popup_max(
                popup_min.x + ImGui::GetWindowWidth(),
                popup_min.y + ImGui::GetWindowHeight());

            draw_framed_box(popup_dl, popup_min, popup_max);

            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, colors::child_fill);
            {
                ImVec4 grab = ImLerp(colors::child_fill, colors::text_inactive, 0.45f);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, grab);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImLerp(grab, colors::accent, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImLerp(grab, colors::accent, 0.55f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);

            ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoBackground;
            if (!need_scroll)
                child_flags |= ImGuiWindowFlags_NoScrollbar;

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("##combo_list", ImVec2(box_w, popup_h), false, child_flags);
            ImDrawList* list_dl = ImGui::GetWindowDrawList();

            if (ImGui::IsWindowAppearing() && need_scroll && *current_item > 0)
            {
                // чуть выше выбранного чтобы видно было соседей
                ImGui::SetScrollY((float)*current_item * item_h - popup_h * 0.35f);
            }

            for (int i = 0; i < items_count; ++i)
            {
                const char* item_text = items[i] ? items[i] : "";
                ImGui::SetCursorPosY(i * item_h);
                char item_id[64];
                ImFormatString(item_id, IM_ARRAYSIZE(item_id), "##ci%d", i);
                if (ImGui::InvisibleButton(item_id, ImVec2(box_w - (need_scroll ? 8.0f : 0.0f), item_h)))
                {
                    *current_item = i;
                    value_changed = true;
                    ImGui::CloseCurrentPopup();
                }

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const bool item_hovered = ImGui::IsItemHovered();
                const bool item_selected = (*current_item == i);

                ImU32 item_col = colors::text_inactive_u32();
                if (item_selected)
                    item_col = colors::accent_u32();

                else if (item_hovered)
                    item_col = colors::text_active_u32();

                const ImVec2 item_tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, item_text);
                const float item_ty = ImFloor(item_min.y + (item_h - item_tsz.y) * 0.5f);
                float item_tx = item_min.x + text_pad;

                if (item_selected)
                {
                    const char* marker = ">";
                    const ImVec2 marker_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, marker);
                    draw_outlined_text(
                        list_dl, font, fs,
                        ImVec2(ImFloor(item_min.x + text_pad), item_ty),
                        item_col, marker);
                    item_tx += marker_sz.x + marker_gap;
                }

                draw_outlined_text(
                    list_dl, font, fs,
                    ImVec2(ImFloor(item_tx), item_ty),
                    item_col, item_text);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        return value_changed;
    }

    static void build_multi_preview(
        char* buffer,
        int buffer_size,
        const bool* selected,
        const char* const items[],
        int items_count) {
        if (!buffer || buffer_size <= 0)
            return;

        buffer[0] = '\0';
        int written = 0;
        int selected_count = 0;

        for (int i = 0; i < items_count; ++i) {
            if (!selected || !selected[i])
                continue;

            const char* item_text = items[i] ? items[i] : "";
            if (!item_text[0])
                continue;

            if (written > 0)
                written += ImFormatString(buffer + written, buffer_size - written, ", ");
            written += ImFormatString(buffer + written, buffer_size - written, "%s", item_text);
            ++selected_count;
        }

        if (selected_count == 0)
            ImFormatString(buffer, buffer_size, "none showing");
    }

    bool multi_combo(const char* label, bool* selected, const char* const items[], int items_count)
    {
        float combo_h = 20.f;
        float label_gap = 4.f;
        float label_ox = 1.f;
        float item_h = 20.f;
        float text_pad = 6.f;
        float marker_gap = 3.f;

        if (!selected || !items || items_count <= 0)
            return false;

        menu_row();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label ? label : "";
        const ImGuiID id = window->GetID(text);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImVec2 label_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text);

        const float row_h = label_sz.y;
        const float bottom_gap = 4.f;
        const float total_h = row_h + label_gap + combo_h + bottom_gap;
        const float box_w = combo_width();
        const ImVec2 total_sz(box_w, total_h);

        const ImVec2 box_min(pos.x, pos.y + row_h + label_gap);
        const ImVec2 box_max(box_min.x + box_w, box_min.y + combo_h);
        const ImRect box_bb(box_min, box_max);
        const ImRect total_bb(pos, pos + total_sz);

        ImGui::ItemSize(total_sz);
        if (!ImGui::ItemAdd(total_bb, id, &box_bb))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);

        char popup_id[128];
        ImFormatString(popup_id, IM_ARRAYSIZE(popup_id), "%s##multipopup", text);
        if (pressed)
            ImGui::OpenPopup(popup_id);

        float* anim = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim = ImLerp(*anim, (hovered || ImGui::IsPopupOpen(popup_id)) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        const ImU32 label_col = colors::label_u32(*anim);

        if (text[0] != '\0')
        {
            draw_outlined_text(
                dl, font, fs,
                ImVec2(ImFloor(pos.x + label_ox), ImFloor(pos.y)),
                label_col, text);
        }

        draw_framed_box(dl, box_min, box_max);

        char preview_buf[128];
        build_multi_preview(preview_buf, IM_ARRAYSIZE(preview_buf), selected, items, items_count);

        const ImVec2 preview_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, preview_buf);
        const float preview_max_w = box_w - text_pad * 2.0f - 12.0f;
        const char* preview_draw = preview_buf;
        char preview_truncated[128];
        if (preview_sz.x > preview_max_w)
        {
            ImFormatString(preview_truncated, IM_ARRAYSIZE(preview_truncated), "%s...", preview_buf);
            preview_draw = preview_truncated;
        }

        const ImVec2 preview_draw_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, preview_draw);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_min.x + text_pad),
                ImFloor(box_min.y + (combo_h - preview_draw_sz.y) * 0.5f)),
            label_col, preview_draw);

        const bool is_open = ImGui::IsPopupOpen(popup_id);
        const char* icon = is_open ? "-" : "+";
        const ImVec2 icon_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, icon);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_max.x - icon_sz.x - text_pad),
                ImFloor(box_min.y + (combo_h - icon_sz.y) * 0.5f)),
            colors::accent_u32(), icon);

        bool value_changed = false;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 1.0f));
        ImGui::SetNextWindowSize(ImVec2(box_w, items_count * item_h));
        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        {
            ImDrawList* popup_dl = ImGui::GetWindowDrawList();
            const ImVec2 popup_min = ImGui::GetWindowPos();
            const ImVec2 popup_max(
                popup_min.x + ImGui::GetWindowWidth(),
                popup_min.y + ImGui::GetWindowHeight());

            draw_framed_box(popup_dl, popup_min, popup_max);

            for (int i = 0; i < items_count; ++i)
            {
                const char* item_text = items[i] ? items[i] : "";
                const bool item_shown = selected[i];

                char item_button_id[128];
                ImFormatString(item_button_id, IM_ARRAYSIZE(item_button_id), "%s##multi_%d", text, i);

                ImGui::SetCursorPosY(i * item_h);
                if (ImGui::InvisibleButton(item_button_id, ImVec2(box_w, item_h)))
                {
                    selected[i] = !selected[i];
                    value_changed = true;
                }

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const bool item_hovered = ImGui::IsItemHovered();

                ImU32 item_col = colors::text_inactive_u32();
                if (item_shown)
                    item_col = colors::accent_u32();

                else if (item_hovered)
                    item_col = colors::text_active_u32();

                const ImVec2 item_tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, item_text);
                const float item_ty = ImFloor(item_min.y + (item_h - item_tsz.y) * 0.5f);
                float item_tx = item_min.x + text_pad;

                if (item_shown)
                {
                    const char* marker = ">";
                    const ImVec2 marker_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, marker);
                    draw_outlined_text(
                        popup_dl, font, fs,
                        ImVec2(ImFloor(item_min.x + text_pad), item_ty),
                        item_col, marker);
                    item_tx += marker_sz.x + marker_gap;
                }

                draw_outlined_text(
                    popup_dl, font, fs,
                    ImVec2(ImFloor(item_tx), item_ty),
                    item_col, item_text);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        return pressed || value_changed;
    }

    bool combo_inline(const char* id, int* current_item, const char* const items[], int items_count, float width)
    {
        const float combo_h = 20.f;
        const float item_h = 20.f;
        const float text_pad = 6.f;
        const float marker_gap = 3.f;

        if (!id || !current_item || !items || items_count <= 0)
            return false;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID wid = window->GetID(id);

        if (*current_item < 0)
            *current_item = 0;
        if (*current_item >= items_count)
            *current_item = items_count - 1;

        float box_w = width;
        if (box_w < 48.f)
            box_w = 48.f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 box_min = pos;
        const ImVec2 box_max(pos.x + box_w, pos.y + combo_h);
        const ImRect box_bb(box_min, box_max);

        ImGui::ItemSize(ImVec2(box_w, combo_h));
        if (!ImGui::ItemAdd(box_bb, wid))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(box_bb, wid, &hovered, &held);

        char popup_id[128];
        ImFormatString(popup_id, IM_ARRAYSIZE(popup_id), "%s##popup", id);
        if (pressed)
            ImGui::OpenPopup(popup_id);

        float* anim = window->StateStorage.GetFloatRef(wid, 0.0f);
        *anim = ImLerp(*anim, (hovered || ImGui::IsPopupOpen(popup_id)) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        const ImU32 label_col = colors::label_u32(*anim);

        draw_framed_box(dl, box_min, box_max);

        if (*anim > 0.01f)
        {
            ImRect hover_fill(
                ImVec2(box_min.x + 2.f, box_min.y + 2.f),
                ImVec2(box_max.x - 2.f, box_max.y - 2.f));
            dl->AddRectFilled(hover_fill.Min, hover_fill.Max, colors::widget_track_hover_u32(*anim));
        }

        const char* preview = items[*current_item] ? items[*current_item] : "";
        const ImVec2 preview_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, preview);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_min.x + text_pad),
                ImFloor(box_min.y + (combo_h - preview_sz.y) * 0.5f)),
            label_col, preview);

        const bool is_open = ImGui::IsPopupOpen(popup_id);
        const char* icon = is_open ? "-" : "+";
        const ImVec2 icon_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, icon);
        draw_outlined_text(
            dl, font, fs,
            ImVec2(
                ImFloor(box_max.x - icon_sz.x - text_pad),
                ImFloor(box_min.y + (combo_h - icon_sz.y) * 0.5f)),
            colors::accent_u32(), icon);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        bool value_changed = false;
        float content_h = items_count * item_h;
        float popup_h = item_h * 8.f;
        if (content_h < popup_h)
            popup_h = content_h;
        const bool need_scroll = content_h > popup_h + 0.5f;

        ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 1.0f));
        ImGui::SetNextWindowSize(ImVec2(box_w, popup_h));
        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
        {
            ImDrawList* popup_dl = ImGui::GetWindowDrawList();
            const ImVec2 popup_min = ImGui::GetWindowPos();
            const ImVec2 popup_max(
                popup_min.x + ImGui::GetWindowWidth(),
                popup_min.y + ImGui::GetWindowHeight());
            draw_framed_box(popup_dl, popup_min, popup_max);

            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, colors::child_fill);
            {
                ImVec4 grab = ImLerp(colors::child_fill, colors::text_inactive, 0.45f);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, grab);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImLerp(grab, colors::accent, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImLerp(grab, colors::accent, 0.55f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);

            ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoBackground;
            if (!need_scroll)
                child_flags |= ImGuiWindowFlags_NoScrollbar;

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("##combo_inline_list", ImVec2(box_w, popup_h), false, child_flags);
            ImDrawList* list_dl = ImGui::GetWindowDrawList();

            for (int i = 0; i < items_count; ++i)
            {
                const char* item_text = items[i] ? items[i] : "";
                ImGui::SetCursorPosY(i * item_h);
                char item_id[64];
                ImFormatString(item_id, IM_ARRAYSIZE(item_id), "##cii%d", i);
                if (ImGui::InvisibleButton(item_id, ImVec2(box_w - (need_scroll ? 8.0f : 0.0f), item_h)))
                {
                    *current_item = i;
                    value_changed = true;
                    ImGui::CloseCurrentPopup();
                }

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const bool item_hovered = ImGui::IsItemHovered();
                const bool item_selected = (*current_item == i);

                ImU32 item_col = colors::text_inactive_u32();
                if (item_selected)
                    item_col = colors::accent_u32();
                else if (item_hovered)
                    item_col = colors::text_active_u32();

                const ImVec2 item_tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, item_text);
                const float item_ty = ImFloor(item_min.y + (item_h - item_tsz.y) * 0.5f);
                float item_tx = item_min.x + text_pad;

                if (item_selected)
                {
                    const char* marker = ">";
                    const ImVec2 marker_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, marker);
                    draw_outlined_text(
                        list_dl, font, fs,
                        ImVec2(ImFloor(item_min.x + text_pad), item_ty),
                        item_col, marker);
                    item_tx += marker_sz.x + marker_gap;
                }

                draw_outlined_text(
                    list_dl, font, fs,
                    ImVec2(ImFloor(item_tx), item_ty),
                    item_col, item_text);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        return value_changed;
    }
}
