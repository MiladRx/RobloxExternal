#include "helpers.h"
#include "../widgets/checkbox.h"
#include "../widgets/colorpicker.h"
#include "../widgets/color_presets.h"
#include "../widgets/color_style.h"
#include "../widgets/keybind.h"
#include "../widgets/select.h"
#include "../widgets/slider.h"
#include "../widgets/dropdown.h"
#include "../widgets/spacing.h"

#include <imgui.h>

void ng_tabs::pad()
{
	ImGui::SetCursorPosX(12.f);
}

void ng_tabs::gap()
{
	ImGui::Dummy(ImVec2(0.f, ng::item_gap));
	pad();
}

void ng_tabs::lab(const char* text)
{
	if (!text) text = "";
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	dl->AddText(p, IM_COL32(220, 226, 236, 230), text);
	ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 4.f));
	pad();
}

bool ng_tabs::row_cb_color(const char* label, bool* v, float col[4], const char* id)
{
	if (!v || !col) return false;

	pad();
	bool ch = ng::checkbox(label, v);

	if (ng::cp_style() == 1)
	{
		ImGui::SetCursorPosX(12.f);
		if (ng::color_presets(id, "color", label, col, *v))
			ch = true;
	}

	else
	{
		if (ng::colorpicker(id, col, *v))
			ch = true;
	}

	return ch;
}

bool ng_tabs::row_cb_color2(
	const char* label,
	bool* v,
	float col_a[4],
	float col_b[4],
	const char* id_a,
	const char* id_b,
	const char* name_a,
	const char* name_b,
	bool colors_always
)
{
	if (!v || !col_a || !col_b) return false;
	if (!name_a) name_a = "outline";
	if (!name_b) name_b = "fill";

	pad();
	bool ch = ng::checkbox(label, v);
	bool show = colors_always ? true : *v;

	if (ng::cp_style() == 1)
	{
		ImGui::SetCursorPosX(12.f);
		if (ng::color_presets(id_a, "color", name_a, col_a, show))
			ch = true;
		ImGui::SetCursorPosX(12.f);
		if (ng::color_presets(id_b, "color", name_b, col_b, show))
			ch = true;
	}

	else
	{
		if (ng::colorpicker(id_a, col_a, show, 1))
			ch = true;
		if (ng::colorpicker(id_b, col_b, show, 0))
			ch = true;
	}

	return ch;
}

void ng_tabs::row_keybind(const char* id, const char* label, int* vk, int* mode)
{
	if (!vk || !mode) return;

	pad();
	ImGui::PushID(id);

	// как чекбокс+keybind: короткий item слева, справа дырка под kb/mode
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = 28.f;
	ImVec2 ts = ImGui::CalcTextSize(label ? label : "");
	float lw = ts.x + 10.f;
	if (lw < 40.f) lw = 40.f;

	ImGui::Dummy(ImVec2(lw, h));
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(p.x, p.y + (h - ts.y) * 0.5f),
		IM_COL32(220, 226, 236, 230),
		label ? label : ""
	);

	ng::keybind("##kb", vk, mode, true);
	ImGui::PopID();
}

bool ng_tabs::row_select(const char* id, const char* label, int* cur, const char* const items[], int count)
{
	if (!cur || !items || count <= 0) return false;
	pad();
	lab(label);
	return ng::select(id, cur, items, count);
}

bool ng_tabs::row_slider(const char* id, const char* label, float* v, float mn, float mx, bool shown)
{
	if (!v) return false;
	pad();
	lab(label);
	return ng::slider(id, v, mn, mx, shown);
}

bool ng_tabs::row_slider_i(const char* id, const char* label, int* v, int mn, int mx, bool shown)
{
	if (!v) return false;
	float f = (float)*v;
	bool ch = row_slider(id, label, &f, (float)mn, (float)mx, shown);
	int n = (int)(f + 0.5f);
	if (n < mn) n = mn;
	if (n > mx) n = mx;
	if (n != *v)
	{
		*v = n;
		ch = true;
	}
	return ch;
}

bool ng_tabs::row_dropdown(const char* id, const char* label, bool* sel, const char* const items[], int count, bool shown)
{
	if (!sel || !items || count <= 0) return false;
	pad();
	lab(label);
	return ng::dropdown(id, sel, items, count, shown);
}
