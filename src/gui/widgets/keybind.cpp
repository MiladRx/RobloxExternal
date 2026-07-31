#include "pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui_internal.h"
#include <Windows.h>
#include <cstdio>

namespace {
    //constexpr float k_key_box_w = 72.0f;
    //constexpr float k_mode_box_w = 56.0f;
    //constexpr float k_key_box_h = 20.0f;
    //constexpr float k_box_gap = 5.0f;
    //constexpr float k_label_gap = 8.0f;
    //constexpr float k_right_margin = 14.0f;
    //constexpr ImU32 k_outline = IM_COL32(0, 0, 0, 255);

    const char* mode_name(int mode) {
        if (mode == 1) return "toggle";
        if (mode == 2) return "always";
        return "hold";
    }

    void key_name(int vk, char* out, int out_size)
    {
        if (vk == 0)
        {
            ImFormatString(out, out_size, "none");
            return;
        }

        if (vk == VK_LBUTTON) { ImFormatString(out, out_size, "mouse1"); return; }
        if (vk == VK_RBUTTON) { ImFormatString(out, out_size, "mouse2"); return; }
        if (vk == VK_MBUTTON) { ImFormatString(out, out_size, "mouse3"); return; }
        if (vk == VK_XBUTTON1) { ImFormatString(out, out_size, "mouse4"); return; }
        if (vk == VK_XBUTTON2) { ImFormatString(out, out_size, "mouse5"); return; }

        UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
        LONG lparam = (LONG)scan << 16;

        // стрелки/home/end и тп, иначе GetKeyNameText врёт
        if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN
            || vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME
            || vk == VK_INSERT || vk == VK_DELETE || vk == VK_DIVIDE || vk == VK_NUMLOCK)
        {
            lparam |= (1 << 24);
        }

        char buf[64] = {};
        if (GetKeyNameTextA(lparam, buf, sizeof(buf)) > 0)
            ImFormatString(out, out_size, "%s", buf);

        else
            ImFormatString(out, out_size, "key%d", vk);
    }

    static bool s_capture_snapshot[256] = {};

    void take_key_snapshot() {
        for (int vk = 1; vk < 256; ++vk)
            s_capture_snapshot[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    // что нажали после старта захвата (лкм игнорим, клик по боксу)
    int find_new_key_since_snapshot() {
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_LBUTTON)
                continue;
            if (!s_capture_snapshot[vk] && (GetAsyncKeyState(vk) & 0x8000))
                return vk;
        }
        return 0;
    }

    bool draw_key_box(const char* id_seed, int* key)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems || !key)
            return false;

        ImGuiID imgui_id = window->GetID(id_seed);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(72.f, 20.f); // key box
        ImRect bb(pos, pos + size);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, imgui_id))
            return false;

        bool hovered = false;
        bool held = false;
        bool pressed = ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);

        bool* capturing = window->StateStorage.GetBoolRef(imgui_id, false);

        if (pressed)
        {
            bool now = !*capturing;
            *capturing = now;
            if (now)
                take_key_snapshot();
        }

        bool changed = false;
        if (*capturing)
        {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                *key = 0;
                *capturing = false;
                changed = true;
            }

            else
            {
                int captured = find_new_key_since_snapshot();
                if (captured != 0)
                {
                    *key = captured;
                    *capturing = false;
                    changed = true;
                }
            }
        }

        dl->AddRectFilled(
            ImVec2(bb.Min.x + 1.f, bb.Min.y + 1.f),
            ImVec2(bb.Max.x - 1.f, bb.Max.y - 1.f),
            colors::widget_track_u32());
        dl->AddRect(
            ImVec2(bb.Min.x + 1.f, bb.Min.y + 1.f),
            ImVec2(bb.Max.x - 1.f, bb.Max.y - 1.f),
            colors::widget_inline_u32(), 0.f, 0, 1.f);
        dl->AddRect(bb.Min, bb.Max, IM_COL32(0, 0, 0, 255), 0.f, 0, 1.f);

        ImFont* font = fonts::ui();
        float fs = fonts::ui_size();

        char text_buf[32];
        if (*capturing)
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "...");

        else
            key_name(*key, text_buf, IM_ARRAYSIZE(text_buf));

        ImU32 text_col = colors::label_u32(hovered ? 1.f : 0.f);
        if (*capturing)
            text_col = colors::accent_u32();
        ImVec2 text_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text_buf);
        widgets::draw_outlined_text(
            dl, font, fs,
            ImVec2(ImFloor(bb.Min.x + (size.x - text_sz.x) * 0.5f),
                   ImFloor(bb.Min.y + (size.y - text_sz.y) * 0.5f)),
            text_col, text_buf);

        return changed;
    }

    bool draw_mode_box(const char* id_seed, int* mode)
    {
        if (!mode)
            return false;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiID imgui_id = window->GetID(id_seed);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(56.f, 20.f); // mode box
        ImRect bb(pos, pos + size);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, imgui_id))
            return false;

        bool hovered = false;
        bool held = false;
        bool pressed = ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);
        if (pressed)
            *mode = (*mode + 1) % 3;

        dl->AddRectFilled(
            ImVec2(bb.Min.x + 1.f, bb.Min.y + 1.f),
            ImVec2(bb.Max.x - 1.f, bb.Max.y - 1.f),
            colors::widget_track_u32());
        dl->AddRect(
            ImVec2(bb.Min.x + 1.f, bb.Min.y + 1.f),
            ImVec2(bb.Max.x - 1.f, bb.Max.y - 1.f),
            colors::widget_inline_u32(), 0.f, 0, 1.f);
        dl->AddRect(bb.Min, bb.Max, IM_COL32(0, 0, 0, 255), 0.f, 0, 1.f);

        ImFont* font = fonts::ui();
        float fs = fonts::ui_size();
        const char* name = mode_name(*mode);
        ImVec2 text_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, name);
        widgets::draw_outlined_text(
            dl, font, fs,
            ImVec2(ImFloor(bb.Min.x + (size.x - text_sz.x) * 0.5f),
                   ImFloor(bb.Min.y + (size.y - text_sz.y) * 0.5f)),
            colors::label_u32(hovered ? 1.f : 0.f), name);

        return pressed;
    }
}

namespace widgets {
    bool keybind(const char* label, int* key, int* activation_mode)
    {
        if (!key)
            return false;

        menu_row();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const char* text = label ? label : "";
        ImFont* font = fonts::ui();
        float fs = fonts::ui_size();
        float key_w = 72.f;
        float mode_w = 56.f;
        float box_h = 20.f;
        float box_gap = 5.f;
        float right_m = 14.f;

        if (text[0] != '\0')
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 label_sz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            draw_outlined_text(
                dl, font, fs,
                ImVec2(ImFloor(pos.x), ImFloor(pos.y + (box_h - label_sz.y) * 0.5f)),
                colors::text_active_u32(), text);
            ImGui::Dummy(ImVec2(label_sz.x, box_h));
            ImGui::SameLine();
        }

        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            float key_area_w = key_w;
            if (activation_mode)
                key_area_w += box_gap + mode_w;
            float right_edge = win->WorkRect.Max.x - right_m;
            float box_left = right_edge - key_area_w;
            ImVec2 cur = ImGui::GetCursorScreenPos();
            if (cur.x < box_left) cur.x = box_left;
            ImGui::SetCursorScreenPos(cur);
        }

        char id_buf[128];
        ImFormatString(id_buf, IM_ARRAYSIZE(id_buf), "%s##keybind", text);
        bool changed = draw_key_box(id_buf, key);

        if (activation_mode)
        {
            ImGui::SameLine(0.f, box_gap);
            char mode_id[128];
            ImFormatString(mode_id, IM_ARRAYSIZE(mode_id), "%s##mode", text);
            changed |= draw_mode_box(mode_id, activation_mode);
        }

        ImGui::SetCursorPosX(k_menu_pad_x);
        return changed;
    }

    bool checkbox_keybind(const char* label, bool* value, int* key, int* activation_mode)
    {
        bool label_changed = checkbox(label, value);
        const float next_y = ImGui::GetCursorPosY();

        float key_w = 72.f;
        float mode_w = 56.f;
        float box_gap = 5.f;
        float right_m = 14.f;

        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            float key_area_w = key_w;
            if (activation_mode)
                key_area_w += box_gap + mode_w;
            float right_edge = win->WorkRect.Max.x - right_m;
            float box_left = right_edge - key_area_w;
            ImGui::SameLine();
            ImVec2 cur = ImGui::GetCursorScreenPos();
            if (cur.x < box_left) cur.x = box_left;
            ImGui::SetCursorScreenPos(cur);
        }

        char id_buf[128];
        ImFormatString(id_buf, IM_ARRAYSIZE(id_buf), "%s##keybind", label ? label : "");
        bool key_changed = draw_key_box(id_buf, key);

        if (activation_mode)
        {
            ImGui::SameLine(0.f, box_gap);
            char mode_id[128];
            ImFormatString(mode_id, IM_ARRAYSIZE(mode_id), "%s##mode", label ? label : "");
            key_changed |= draw_mode_box(mode_id, activation_mode);
        }

        ImGui::SetCursorPosY(next_y);
        ImGui::SetCursorPosX(k_menu_pad_x);

        if (value && key && *key != 0)
        {
            int mode = activation_mode ? *activation_mode : 0;
            if (mode == 1)
            {
                if (GetAsyncKeyState(*key) & 1)
                    *value = !*value;
            }

            else
            {
                *value = (GetAsyncKeyState(*key) & 0x8000) != 0;
            }
        }

        return label_changed || key_changed;
    }
}
