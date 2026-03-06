#pragma once

#include <string>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

std::string makeDebugPath(const std::string& tag);

void saveDebugFrame(const cv::Mat& bgr, const std::string& tag);
void saveDebugFrame(const cv::Mat& bgr, const std::string& tag, const cv::Rect& r1, const cv::Scalar& c1 = cv::Scalar(0, 0, 255));
void saveDebugFrame(const cv::Mat& bgr, const std::string& tag, const cv::Rect& r1, const cv::Rect& r2);
void saveDebugFrame(const cv::Mat& bgr, const std::string& tag, const cv::Rect& r1, const cv::Rect& r2, const cv::Rect& r3);
