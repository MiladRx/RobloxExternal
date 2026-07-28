#include "pch.h"
#define NOMINMAX
#include "preview/PreviewRenderer.h"
#include "app/Graphics.h"
#include "gui/colors/colors.h"
#include <stb_image.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

#include "preview/PreviewShaders.inl"

namespace Cheat::Core {

#include "preview/PreviewInit.inl"
#include "preview/PreviewModel.inl"
#include "preview/PreviewUpdate.inl"
#include "preview/PreviewProject.inl"

}
