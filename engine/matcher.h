#pragma once
#include <vector>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "template_store.h"
#include "../core/types.h"

cv::Rect clampRect(cv::Rect r, const cv::Size& bounds);
cv::Rect centerThirdStripRoi(const cv::Size& bounds);
TplMatch matchBest(const cv::Mat& srcGray, const GrayTpl& tpl, int defaultMethod = cv::TM_CCOEFF_NORMED);
TplMatch matchBestRoi(const cv::Mat& srcGray, const GrayTpl& tpl, cv::Rect roi, int method = cv::TM_CCOEFF_NORMED);
TplMatch matchBestRoiAtScaleAndAngle(const cv::Mat& srcGray, const GrayTpl& tpl, cv::Rect roi, double scale, double angleDeg, int method = cv::TM_CCOEFF_NORMED);
TplMatch matchBestRoiMultiScale(const cv::Mat& srcGray, const GrayTpl& tpl, cv::Rect roi, int method = cv::TM_CCOEFF_NORMED, double* bestScaleOut = nullptr);
TplMatch matchBestRoiTrackBarAutoScale(const cv::Mat& srcGray, const GrayTpl& tpl, cv::Rect roi, int method = cv::TM_CCOEFF_NORMED, double* bestScaleOut = nullptr, double* bestAngleOut = nullptr);
