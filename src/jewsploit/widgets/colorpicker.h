#pragma once

namespace ng
{
	// col = rgba 0..1. shown=false -> прячется. попап поверх всего
	// slot: 0 = крайний справа, 1 = левее и тд
	// show_swatch=false -> тока попап (для кисти в presets)
	// open_now=true -> открыть попап в этом кадре
	bool colorpicker(const char* id, float col[4], bool shown, int slot = 0, bool show_swatch = true, bool open_now = false);

	// прошлого кадра был открыт попап (для search dim и тп)
	bool colorpicker_any_open();
}