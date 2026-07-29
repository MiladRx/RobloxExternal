#include "checkbox.h"
#include "../colors/colors.h"
#include "../animation/animation.h"

#include <imgui.h>

namespace
{
	ImU32 mix_u32(ImU32 a, ImU32 b, float t)
	{
		if (t < 0.f) t = 0.f;
		if (t > 1.f) t = 1.f;

		ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
		ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
		ImVec4 c;
		c.x = ca.x + (cb.x - ca.x) * t;
		c.y = ca.y + (cb.y - ca.y) * t;
		c.z = ca.z + (cb.z - ca.z) * t;
		c.w = ca.w + (cb.w - ca.w) * t;
		return ImGui::ColorConvertFloat4ToU32(c);
	}
}

bool ng::checkbox(const char* label, bool* v)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();

	float box = 18.f;
	float rnd = 5.f;
	float gap = 10.f;

	ImVec2 ts = ImGui::CalcTextSize(label);
	float h = box;
	if (ts.y > h)
	{
		h = ts.y;
	}

	float w = box + gap + ts.x + 6.f;

	ImGui::InvisibleButton(label, ImVec2(w, h + 2.f));
	bool hit = ImGui::IsItemClicked();

	if (hit)
	{
		*v = !*v;
	}

	ImGuiID id = ImGui::GetItemID();
	ImGuiStorage* st = ImGui::GetStateStorage();
	float t = st->GetFloat(id, *v ? 1.f : 0.f);
	float want = *v ? 1.f : 0.f;
	t = anim::approach(t, want, 7.f, io.DeltaTime);
	st->SetFloat(id, t);

	float e = anim::ease_out_cubic(t);

	float by = pos.y + (h - box) * 0.5f + 3.f;
	ImU32 bg = mix_u32(col::checkbox_off_u32(), col::checkbox_on_u32(), e);
	ImU32 br = mix_u32(IM_COL32(255, 255, 255, 28), IM_COL32(255, 255, 255, 48), e);

	dl->AddRectFilled(ImVec2(pos.x, by), ImVec2(pos.x + box, by + box), bg, rnd);
	dl->AddRect(ImVec2(pos.x, by), ImVec2(pos.x + box, by + box), br, rnd, 0, 1.f);

	float ty = pos.y + (h - ts.y) * 0.5f + 3.f;
	dl->AddText(ImVec2(pos.x + box + gap, ty), IM_COL32(220, 226, 236, 230), label);

	return hit;
}