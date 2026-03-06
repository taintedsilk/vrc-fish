#pragma once

#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

// Grayscale template with optional alpha mask for masked matchTemplate.
struct GrayTpl {
	cv::Mat gray;
	cv::Mat mask;  // non-empty when masked matchTemplate is enabled
	bool empty() const { return gray.empty(); }
	int cols() const { return gray.cols; }
	int rows() const { return gray.rows; }
	cv::Size size() const { return gray.size(); }
};

// All loaded template images used for detection.
struct TemplateStore {
	GrayTpl biteExclBottom;
	GrayTpl biteExclFull;
	GrayTpl minigameBarFull;
	GrayTpl fishIcon;
	GrayTpl fishIconAlt;
	GrayTpl fishIconAlt2;
	std::vector<GrayTpl> fishIcons;           // fish_icon + fish_icon_alt*.png (dynamic)
	std::vector<std::string> fishIconFiles;   // corresponding filenames for logging
	GrayTpl playerSlider;
};

// Template loading functions
GrayTpl loadGrayTplFromFile(const std::string& path);
GrayTpl tryLoadGrayTplFromFile(const std::string& path);

// Fish icon file discovery
bool isFishAltIconFilename(const std::string& file);
int parseFishAltIconIndex(const std::string& file);
std::vector<std::string> listFishAltIconFiles(const std::string& dir);

// Load all templates into a TemplateStore from the configured resource directory.
// Returns false if required templates are missing.
bool loadTemplateStore(TemplateStore& store);
