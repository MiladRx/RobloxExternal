#pragma once

namespace ng
{
	// shown=false -> hides with animation; fill/grab animate too
	bool slider(const char* id, float* v, float mn, float mx, bool shown);
}