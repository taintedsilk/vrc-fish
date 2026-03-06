#define NOMINMAX
#include "template_store.h"
#include "../gui/gui_config_def.h"
#include "../gui/gui_logger.h"
#include "../runtime/params.h"
#include "../infra/fs/path_utils.h"

#include <windows.h>
#include <algorithm>
#include <vector>
#include <string>

using cv::Mat;
using cv::imread;
using cv::cvtColor;
using cv::split;
using cv::threshold;
using cv::minMaxLoc;
using cv::IMREAD_UNCHANGED;
using cv::COLOR_BGRA2BGR;
using cv::COLOR_BGR2GRAY;
using cv::THRESH_BINARY;

// ---------------------------------------------------------------------------
// Template loading
// ---------------------------------------------------------------------------

GrayTpl loadGrayTplFromFile(const std::string& path) {
	Mat raw = imread(path, IMREAD_UNCHANGED);
	if (raw.empty()) {
		LOG_ERROR("Template load failed: %s", path.c_str());
		return GrayTpl{};
	}
	GrayTpl tpl{};
	if (raw.channels() == 4) {
		std::vector<Mat> channels;
		split(raw, channels);
		Mat alpha = channels[3];
		double minA = 0, maxA = 0;
		minMaxLoc(alpha, &minA, &maxA);
		Mat bgr;
		cvtColor(raw, bgr, COLOR_BGRA2BGR);
		cvtColor(bgr, tpl.gray, COLOR_BGR2GRAY);
		if (minA < 255.0) {
			threshold(alpha, tpl.mask, 0, 255, THRESH_BINARY);
			LOG_INFO("Template loaded (with mask): %s", path.c_str());
		} else {
			LOG_INFO("Template loaded: %s", path.c_str());
		}
	} else {
		Mat bgr = raw;
		if (raw.channels() == 1) {
			tpl.gray = raw;
		} else {
			cvtColor(bgr, tpl.gray, COLOR_BGR2GRAY);
		}
		LOG_INFO("Template loaded: %s", path.c_str());
	}
	return tpl;
}

GrayTpl tryLoadGrayTplFromFile(const std::string& path) {
	Mat raw = imread(path, IMREAD_UNCHANGED);
	if (raw.empty()) {
		LOG_WARN("Template load failed (ignored): %s", path.c_str());
		return GrayTpl{};
	}
	GrayTpl tpl{};
	if (raw.channels() == 4) {
		std::vector<Mat> channels;
		split(raw, channels);
		Mat alpha = channels[3];
		double minA = 0, maxA = 0;
		minMaxLoc(alpha, &minA, &maxA);
		Mat bgr;
		cvtColor(raw, bgr, COLOR_BGRA2BGR);
		cvtColor(bgr, tpl.gray, COLOR_BGR2GRAY);
		if (minA < 255.0) {
			threshold(alpha, tpl.mask, 0, 255, THRESH_BINARY);
			LOG_INFO("Template loaded (with mask): %s", path.c_str());
		} else {
			LOG_INFO("Template loaded: %s", path.c_str());
		}
	} else {
		Mat bgr = raw;
		if (raw.channels() == 1) {
			tpl.gray = raw;
		} else {
			cvtColor(bgr, tpl.gray, COLOR_BGR2GRAY);
		}
		LOG_INFO("Template loaded: %s", path.c_str());
	}
	return tpl;
}

// ---------------------------------------------------------------------------
// Filename helpers
// ---------------------------------------------------------------------------

static std::string toLowerAscii(std::string s) {
	for (char& c : s) {
		if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	}
	return s;
}

bool isFishAltIconFilename(const std::string& file) {
	std::string f = toLowerAscii(file);
	const std::string prefix = "fish_icon_alt";
	const std::string suffix = ".png";
	if (f.size() < prefix.size() + suffix.size()) return false;
	if (f.rfind(prefix, 0) != 0) return false;
	if (f.compare(f.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
	std::string mid = f.substr(prefix.size(), f.size() - prefix.size() - suffix.size());
	if (mid.empty()) return true;
	for (char c : mid) {
		if (c < '0' || c > '9') return false;
	}
	return true;
}

int parseFishAltIconIndex(const std::string& file) {
	std::string f = toLowerAscii(file);
	const std::string prefix = "fish_icon_alt";
	const std::string suffix = ".png";
	if (!isFishAltIconFilename(file)) return -1;
	std::string mid = f.substr(prefix.size(), f.size() - prefix.size() - suffix.size());
	if (mid.empty()) return -1;
	int v = 0;
	for (char c : mid) {
		if (c < '0' || c > '9') return -1;
		int d = (int)(c - '0');
		if (v > 100000000) return -1;
		v = v * 10 + d;
	}
	return v;
}

// ---------------------------------------------------------------------------
// File listing
// ---------------------------------------------------------------------------

static std::vector<std::string> listFilesByWildcard(const std::string& dir, const std::string& wildcard) {
	std::vector<std::string> out;
	std::string query = infra::fs::joinPath(dir, wildcard);
	WIN32_FIND_DATAA ffd{};
	HANDLE hFind = FindFirstFileA(query.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return out;
	}
	do {
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		if (ffd.cFileName[0] == '\0') {
			continue;
		}
		out.emplace_back(ffd.cFileName);
	} while (FindNextFileA(hFind, &ffd));
	FindClose(hFind);

	std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
		bool aOk = isFishAltIconFilename(a);
		bool bOk = isFishAltIconFilename(b);
		if (aOk != bOk) return aOk;
		if (!aOk) return toLowerAscii(a) < toLowerAscii(b);
		int ai = parseFishAltIconIndex(a);
		int bi = parseFishAltIconIndex(b);
		if (ai != bi) {
			if (ai < 0) return true;
			if (bi < 0) return false;
			return ai < bi;
		}
		return toLowerAscii(a) < toLowerAscii(b);
	});
	out.erase(std::unique(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
		return toLowerAscii(a) == toLowerAscii(b);
	}), out.end());

	return out;
}

std::vector<std::string> listFishAltIconFiles(const std::string& dir) {
	std::vector<std::string> all = listFilesByWildcard(dir, "fish_icon_alt*.png");
	std::vector<std::string> result;
	for (const std::string& f : all) {
		if (isFishAltIconFilename(f)) {
			result.push_back(f);
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// Load all templates into a TemplateStore
// ---------------------------------------------------------------------------

bool loadTemplateStore(TemplateStore& store) {

	store.biteExclBottom = loadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, config.tpl_bite_exclamation_bottom));
	store.biteExclFull   = loadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, config.tpl_bite_exclamation_full));
	store.minigameBarFull = loadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, config.tpl_minigame_bar_full));
	store.playerSlider   = loadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, config.tpl_player_slider));

	// Fish icon templates: fish_icon + fish_icon_alt*.png
	store.fishIcons.clear();
	store.fishIconFiles.clear();
	std::vector<std::string> seenFishFilesLower;
	auto addFishTplFile = [&](const std::string& file, bool required, GrayTpl* legacyOut) {
		if (file.empty()) return;
		std::string key = toLowerAscii(file);
		if (std::find(seenFishFilesLower.begin(), seenFishFilesLower.end(), key) != seenFishFilesLower.end()) {
			return;
		}
		GrayTpl tpl = required
			? loadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, file))
			: tryLoadGrayTplFromFile(infra::fs::joinPath(config.resource_dir, file));
		if (tpl.empty()) {
			return;
		}
		seenFishFilesLower.push_back(key);
		if (legacyOut) *legacyOut = tpl;
		store.fishIcons.push_back(tpl);
		store.fishIconFiles.push_back(file);
	};

	// Primary template: must exist
	addFishTplFile(config.tpl_fish_icon, true, &store.fishIcon);
	// Legacy config compat: optional
	addFishTplFile(config.tpl_fish_icon_alt, false, &store.fishIconAlt);
	addFishTplFile(config.tpl_fish_icon_alt2, false, &store.fishIconAlt2);
	// Auto-scan directory: fish_icon_alt<digits>.png / fish_icon_alt.png
	for (const std::string& f : listFishAltIconFiles(config.resource_dir)) {
		addFishTplFile(f, false, nullptr);
	}

	if (store.fishIcons.empty()) {
		LOG_ERROR("No fish icon templates loaded. Check Resource-VRChat/ and tpl_fish_icon config.");
		return false;
	}
	LOG_INFO("Templates loaded: %zu fish icons", store.fishIcons.size());
	return true;
}
