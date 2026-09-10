#pragma once

namespace ng
{
	// title/desc on top, bar: circles + hex. show_brush — brush with colorpicker
	bool color_presets(const char* id, const char* title, const char* desc, float col[4], bool shown = true, bool show_brush = true);
}