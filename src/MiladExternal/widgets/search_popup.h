#pragma once

namespace ng
{
	struct search_entry_t
	{
		const char* name;
		int id;
	};

	using search_draw_fn = void (*)(int id);

	// draw_feature — draws the selected feature's settings right in the popup
	void search_popup(bool* open, const search_entry_t* items, int count, search_draw_fn draw_feature);
}
