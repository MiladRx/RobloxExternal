#pragma once

namespace ng
{
	// mode: 0 hold, 1 toggle (like in the cheat)
	// drawn on the right in a row with the previous control (checkbox)
	bool keybind(const char* id, int* vk, int* mode, bool shown);
}