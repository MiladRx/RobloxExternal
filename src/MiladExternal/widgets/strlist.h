#pragma once

namespace ng
{
	// height for max_vis rows (+ header/paddings)
	float strlist_h(int max_vis = 5);

	// list, shows max_vis, then scroll
	bool strlist(
		const char* id,
		const char* title,
		char items[][128],
		int count,
		int* sel,
		float w,
		int max_vis = 5
	);
}