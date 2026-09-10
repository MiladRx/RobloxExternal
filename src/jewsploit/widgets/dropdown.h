#pragma once

namespace ng
{
	// multi select. shown=false -> hides like slider
	bool dropdown(const char* id, bool* sel, const char* const items[], int count, bool shown);
}