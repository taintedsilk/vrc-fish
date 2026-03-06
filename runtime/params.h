#pragma once

#include <windows.h>
#include "../engine/template_store.h"

// Runtime parameters: window handle, rect, and loaded templates.
// Global instance defined in vrc-fish.cpp.
struct g_params {
	// Alias for backwards compatibility with code that uses g_params::GrayTpl
	using GrayTpl = ::GrayTpl;

	HWND hwnd{};
	RECT rect{};
	bool pause{};

	GrayTpl tpl_vr_bite_excl_bottom;
	GrayTpl tpl_vr_bite_excl_full;
	GrayTpl tpl_vr_minigame_bar_full;
	GrayTpl tpl_vr_fish_icon;
	GrayTpl tpl_vr_fish_icon_alt;
	GrayTpl tpl_vr_fish_icon_alt2;
	std::vector<GrayTpl> tpl_vr_fish_icons;
	std::vector<std::string> tpl_vr_fish_icon_files;
	GrayTpl tpl_vr_player_slider;
};

extern g_params params;
