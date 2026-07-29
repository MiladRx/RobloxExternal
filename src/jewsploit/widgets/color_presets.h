#pragma once

namespace ng
{
	// title/desc сверху, бар: кисть + кружки + hex. кисть открывает colorpicker
	bool color_presets(const char* id, const char* title, const char* desc, float col[4], bool shown = true);
}