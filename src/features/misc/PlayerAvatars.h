#pragma once

#include <cstdint>
#include <d3d11.h>
#include <string>

namespace Cheat {
namespace Features {
namespace PlayerAvatars {

	void Tick();
	void Clear();

	ID3D11ShaderResourceView* Get(std::int64_t user_id);

	// from name→uid cache (0 if not resolved yet)
	std::int64_t LookupUserId(const std::string& username);

}
}
}
