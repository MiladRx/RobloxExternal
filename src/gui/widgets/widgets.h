#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "tabs.h"

namespace widgets {
    // единый отступ рядов меню — вызывается из checkbox/combo/slider/keybind
    inline constexpr float k_menu_pad_x = 12.0f;
    inline constexpr float k_menu_row_pull = 5.0f;
    inline constexpr float k_menu_top_pad = 4.0f;   // первый ряд: вниз от бордера чайлда
    inline constexpr float k_menu_row_h = 21.0f;    // высота ряда = checkbox

    void menu_row_reset();
    void menu_row();
    // лейбл + колорпикер справа, те же отступы что у checkbox
    bool menu_color_row(const char* label, float color[4], const char* id);

    struct text_span {
        const char* text;
        ImU32 color;
    };

    void draw_outlined_text(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text, float extra_width = 0.0f);
    void draw_outlined_text_spans(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, const text_span* spans, int span_count);

    void draw_outlined_text_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        ImU32 base_color,
        ImU32 highlight_color,
        const char* text,
        float phase,
        float wavelength_px);

    void draw_outlined_text_spans_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        const text_span* spans,
        int span_count,
        ImU32 highlight_color,
        float phase,
        float wavelength_px);

    bool checkbox(const char* label, bool* value);
    // без menu_row — для тулбаров / SameLine
    bool checkbox_inline(const char* label, bool* value);
    bool button(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
    // крестик в рамке виджета (как button)
    bool icon_close(const char* id, const ImVec2& size = ImVec2(14.0f, 14.0f));

    bool input_text(const char* id, const char* hint, char* buffer, int buffer_size,
                    float width = 0.0f, ImGuiInputTextFlags flags = 0);
    bool slider_float(const char* label, float* value, float v_min, float v_max, const char* format = "%.3f");
    bool slider_int(const char* label, int* value, int v_min, int v_max, const char* format = "%d");
    bool combo(const char* label, int* current_item, const char* const items[], int items_count);
    // компактный дроп без лейбла / menu_row
    bool combo_inline(const char* id, int* current_item, const char* const items[], int items_count, float width = 80.0f);
    bool multi_combo(const char* label, bool* selected, const char* const items[], int items_count);

    float color_picker_width();
    float color_picker_row_y();
    void same_line_color_picker(float row_y, int slot_from_right, int slot_count);
    bool color_edit4(const char* id, float col[4]);

    bool keybind(const char* label, int* key, int* activation_mode = nullptr);
    bool checkbox_keybind(const char* label, bool* value, int* key, int* activation_mode = nullptr);

    bool begin_child_panel(
        const char* id,
        const ImVec2& size,
        const char* title = nullptr,
        ImFont* title_font = nullptr,
        float title_font_size = 0.0f,
        const ImVec4* outer_border = nullptr,
        const ImVec4* inner_border = nullptr,
        const ImVec4* fill = nullptr);
    void end_child_panel();

    void watermark(float alpha = 1.0f);
    // хитбокс ватермарки с прошлого кадра, click-through
    bool watermark_hit_test(float x, float y);
}
