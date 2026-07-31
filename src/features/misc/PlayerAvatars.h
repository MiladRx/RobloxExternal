#pragma once

#include <cstdint>
#include <d3d11.h>

namespace Cheat {
namespace Features {
namespace PlayerAvatars {

	void Tick();
	void Clear();

	// srv или null
	ID3D11ShaderResourceView* Get(std::int64_t user_id);

}
}
}
