#pragma once

namespace ng
{
	// micro-child with ARGB. lmb = copy, double click = edit/paste
	// w <= 0 -> to the right edge of the window with pad 12
	bool argb_field(const char* id, float col[4], float w = -1.f);
}