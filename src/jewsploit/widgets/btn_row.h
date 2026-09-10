#pragma once

namespace ng
{
	// row of equal buttons like load/delete/save. returns click index or -1
	int btn_row(const char* id, const char* const labels[], int n, float h = 30.f);
}