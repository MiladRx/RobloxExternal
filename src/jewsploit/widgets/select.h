#pragma once

namespace ng
{
	// single select dropdown. shown=false -> collapses like slider
	// close_on_pick=false — list stays open, can keep clicking
	bool select(const char* id, int* cur, const char* const items[], int count, bool shown = true, bool close_on_pick = true);
}
