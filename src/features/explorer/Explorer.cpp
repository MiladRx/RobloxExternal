#include "pch.h"
#include "Explorer.h"
#include "jewsploit/explorer_ui.h"

namespace Cheat::Features {

void Explorer::Initialize()
{
	ng_explorer::init_icons();
}

void Explorer::Shutdown()
{
	ng_explorer::shutdown();
}

void Explorer::Render(float alpha)
{
	ng_explorer::draw(alpha);
}

}
