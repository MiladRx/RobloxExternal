#pragma once

namespace ng
{
	// col = rgba 0..1. shown=false -> hides. popup on top of everything
	// slot: 0 = rightmost, 1 = further left etc
	// show_swatch=false -> popup only (for brush in presets)
	// open_now=true -> open popup this frame
	// show_alpha=false — no transparency strip, a always 1
	bool colorpicker(const char* id, float col[4], bool shown, int slot = 0, bool show_swatch = true, bool open_now = false, bool show_alpha = true);

	// popup was open in the previous frame (for search dim etc)
	bool colorpicker_any_open();
}