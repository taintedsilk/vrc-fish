#pragma once

#include <opencv2/core/types.hpp>

enum class VrFishState {
	Cast,
	WaitBite,
	EnterMinigame,
	ControlMinigame,
	PostMinigame,
};

struct TplMatch {
	cv::Point topLeft{};
	cv::Point center{};
	cv::Rect rect{};
	double score = 0.0;
};

struct FishSliderResult {
	int fishX{}, fishY{};
	int sliderCenterX{}, sliderCenterY{};
	int sliderTop{}, sliderBottom{};
	int sliderHeight{};
	double fishScore{}, sliderScore{};
	bool hasBounds{};
};
