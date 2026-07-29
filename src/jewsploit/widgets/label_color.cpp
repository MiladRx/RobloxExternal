#include "label_color.h"
#include "colorpicker.h"
#include "color_presets.h"
#include "color_style.h"

#include <imgui.h>

bool ng::label_color(const char* id, const char* label, float col[4])
{
	if (!id || !label || !col)
	{
		return false;
	}

	ImGui::PushID(id);

	if (ng::cp_style() == 1)
	{
		bool ch = ng::color_presets("##p", label, nullptr, col, true);
		ImGui::PopID();
		return ch;
	}

	float pad_r = 12.f;
	float sw = 22.f;
	float gap = 8.f;
	float row_h = 28.f;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	// не жрём зону свотча а то клик не доходит
	float label_w = right - pad_r - sw - gap - pos.x;
	if (label_w < 40.f) label_w = 40.f;

	ImGui::InvisibleButton("##row", ImVec2(label_w, row_h));

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 lts = ImGui::CalcTextSize(label);
	float mid_y = pos.y + row_h * 0.5f;

	dl->AddText(
		ImVec2(pos.x, mid_y - lts.y * 0.5f),
		IM_COL32(220, 226, 236, 230),
		label
	);

	bool ch = ng::colorpicker("##cp", col, true, 0);

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));
	ImGui::Dummy(ImVec2(0.f, 0.01f));

	ImGui::PopID();
	return ch;
}