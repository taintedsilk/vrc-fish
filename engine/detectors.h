#pragma once

#include <opencv2/core/mat.hpp>
#include "../core/types.h"

bool detectBite(const cv::Mat& gray, TplMatch* matchOut = nullptr);

bool detectSliderBounds(const cv::Mat& gray, int barX, const cv::Rect& searchRoi,
	int* sliderTopOut, int* sliderBottomOut, int* sliderCenterYOut,
	int brightnessThresh = 180, int minSliderHeight = 15);

bool detectSliderBoundsWide(const cv::Mat& gray, const cv::Rect& searchRoi,
	int* sliderTopOut, int* sliderBottomOut, int* sliderCenterYOut,
	int brightnessThresh = 180, int minSliderHeight = 15);

bool fillFishSliderResult(const cv::Mat& gray, const cv::Rect& roi, const TplMatch& fish, const TplMatch& slider,
	double trackAngleDeg, int fishTplHeightHint, FishSliderResult* result);

bool detectFishAndSliderFast(const cv::Mat& gray, const cv::Rect& barRect, FishSliderResult* result,
	double trackScale, double trackAngleDeg, int cachedFishTplIdx);

bool detectFishAndSliderFull(const cv::Mat& gray, const cv::Rect& barRect, FishSliderResult* result,
	double trackScale, double trackAngleDeg, int* bestTplIdxOut);
