#define NOMINMAX
#include "detectors.h"
#include "matcher.h"
#include "../runtime/params.h"
#include "../gui/gui_config_def.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>

using namespace cv;

extern g_config config;

static TplMatch matchBestMultiScale(const Mat& gray, const GrayTpl& tpl,
	double scaleMin, double scaleMax, double scaleStep) {
	TplMatch best{};
	if (tpl.empty() || gray.empty()) return best;

	// If range is degenerate, just do single-scale
	if (!std::isfinite(scaleMin) || !std::isfinite(scaleMax) || !std::isfinite(scaleStep)
		|| scaleStep <= 0.0 || scaleMin <= 0.0 || scaleMax <= 0.0 || scaleMin > scaleMax) {
		return matchBest(gray, tpl);
	}

	for (double s = scaleMin; s <= scaleMax + 1e-9; s += scaleStep) {
		GrayTpl scaled{};
		if (std::abs(s - 1.0) < 1e-6) {
			scaled.gray = tpl.gray;
			scaled.mask = tpl.mask;
		} else {
			int tw = std::max(1, (int)std::round(tpl.gray.cols * s));
			int th = std::max(1, (int)std::round(tpl.gray.rows * s));
			resize(tpl.gray, scaled.gray, Size(tw, th), 0, 0, INTER_AREA);
			if (!tpl.mask.empty()) {
				resize(tpl.mask, scaled.mask, Size(tw, th), 0, 0, INTER_NEAREST);
			}
		}
		if (gray.cols < scaled.gray.cols || gray.rows < scaled.gray.rows) continue;
		TplMatch m = matchBest(gray, scaled);
		if (m.score > best.score) {
			best = m;
		}
	}
	return best;
}

bool detectBite(const Mat& gray, TplMatch* matchOut) {
	TplMatch m = matchBestMultiScale(gray, params.tpl_vr_bite_excl_bottom,
		config.bite_scale_min, config.bite_scale_max, config.bite_scale_step);
	if (m.score < config.bite_threshold) {
		TplMatch m2 = matchBestMultiScale(gray, params.tpl_vr_bite_excl_full,
			config.bite_scale_min, config.bite_scale_max, config.bite_scale_step);
		if (m2.score > m.score) {
			m = m2;
		}
	}
	if (matchOut) *matchOut = m;
	return m.score >= config.bite_threshold;
}

// ── 颜色検出：在竖条上找到玩家滑块（亮色区域）的上下边界 ──
// 在 barX 附近的窄列中，用亮度阈值找最长连续亮色段作为滑块
// 返回 sliderTop, sliderBottom（全图坐标），成功返回 true
bool detectSliderBounds(const Mat& gray, int barX, const Rect& searchRoi,
	int* sliderTopOut, int* sliderBottomOut, int* sliderCenterYOut,
	int brightnessThresh, int minSliderHeight) {

	// 取 barX 附近 ±halfW 像素宽的竖条取中值，抗噪声
	int halfW = config.slider_detect_half_width;
	if (halfW < 0) halfW = 0;
	int x1 = max(searchRoi.x, barX - halfW);
	int x2 = min(searchRoi.x + searchRoi.width, barX + halfW + 1);
	if (x2 <= x1) return false;

	int y1 = searchRoi.y;
	int y2 = searchRoi.y + searchRoi.height;
	if (y1 < 0) y1 = 0;
	if (y2 > gray.rows) y2 = gray.rows;
	if (y2 <= y1) return false;

	// 对每行取该窄条的平均亮度，收集所有亮色段
	struct BrightRun { int start; int len; };
	std::vector<BrightRun> runs;
	int curRunStart = -1, curRunLen = 0;

	for (int y = y1; y < y2; y++) {
		const uchar* row = gray.ptr<uchar>(y);
		int sum = 0;
		for (int x = x1; x < x2; x++) {
			sum += row[x];
		}
		int avg = sum / (x2 - x1);

		if (avg >= brightnessThresh) {
			if (curRunStart < 0) curRunStart = y;
			curRunLen = y - curRunStart + 1;
		} else {
			if (curRunLen > 0) {
				runs.push_back({ curRunStart, curRunLen });
			}
			curRunStart = -1;
			curRunLen = 0;
		}
	}
	// 收尾
	if (curRunLen > 0) {
		runs.push_back({ curRunStart, curRunLen });
	}

	if (runs.empty()) return false;

	// 合并相邻亮色段：如果两段间隔 <= maxGap 像素，视为同一滑块（鱼图标造成的分割）
	int maxGap = config.slider_detect_merge_gap;
	if (maxGap < 0) maxGap = 0;
	std::vector<BrightRun> merged;
	merged.push_back(runs[0]);
	for (size_t i = 1; i < runs.size(); i++) {
		BrightRun& last = merged.back();
		int lastEnd = last.start + last.len;
		int gap = runs[i].start - lastEnd;
		if (gap <= maxGap) {
			// 合并：扩展到包含新段
			last.len = (runs[i].start + runs[i].len) - last.start;
		} else {
			merged.push_back(runs[i]);
		}
	}

	// 从合并后的段中找最长的
	int bestIdx = 0;
	for (size_t i = 1; i < merged.size(); i++) {
		if (merged[i].len > merged[bestIdx].len) bestIdx = (int)i;
	}
	int bestRunStart = merged[bestIdx].start;
	int bestRunLen = merged[bestIdx].len;

	if (bestRunLen < minSliderHeight) return false;
	// 防止误把整条轨道（或背景高亮）当成滑块
	if (bestRunLen >= (int)((y2 - y1) * 0.95)) return false;

	if (sliderTopOut) *sliderTopOut = bestRunStart;
	if (sliderBottomOut) *sliderBottomOut = bestRunStart + bestRunLen;
	if (sliderCenterYOut) *sliderCenterYOut = bestRunStart + bestRunLen / 2;
	return true;
}

// ── 颜色検出（整段扫）：在整个轨道 ROI 内找滑块亮段的上下边界 ──
// 思路：对 ROI 的每一行统计"亮色像素数量"，连续超过阈值的区间视为滑块
// 这样就不依赖滑块模板给出的 barX，也更不怕被鱼遮挡导致模板偏移
bool detectSliderBoundsWide(const Mat& gray, const Rect& searchRoi,
	int* sliderTopOut, int* sliderBottomOut, int* sliderCenterYOut,
	int brightnessThresh, int minSliderHeight) {

	Rect roi = clampRect(searchRoi, gray.size());
	if (roi.width <= 0 || roi.height <= 0) return false;

	int x1 = roi.x;
	int x2 = roi.x + roi.width;
	int y1 = roi.y;
	int y2 = roi.y + roi.height;
	if (x2 <= x1 || y2 <= y1) return false;

	// 每行至少需要多少个"足够亮"的像素才算滑块范围
	// ROI 宽度通常在 40~60 左右，滑块宽度约占其中一半；这里取一个较保守的比例
	int minRowBright = roi.width / 8; // 约 12.5%
	if (minRowBright < 4) minRowBright = 4;
	if (minRowBright > roi.width) minRowBright = roi.width;

	struct BrightRun { int start; int len; };
	std::vector<BrightRun> runs;
	int curRunStart = -1, curRunLen = 0;

	for (int y = y1; y < y2; y++) {
		const uchar* row = gray.ptr<uchar>(y);
		int brightCnt = 0;
		for (int x = x1; x < x2; x++) {
			if (row[x] >= brightnessThresh) {
				brightCnt++;
			}
		}

		if (brightCnt >= minRowBright) {
			if (curRunStart < 0) curRunStart = y;
			curRunLen = y - curRunStart + 1;
		} else {
			if (curRunLen > 0) {
				runs.push_back({ curRunStart, curRunLen });
			}
			curRunStart = -1;
			curRunLen = 0;
		}
	}
	if (curRunLen > 0) {
		runs.push_back({ curRunStart, curRunLen });
	}
	if (runs.empty()) return false;

	// 合并相邻亮段（鱼图标挡住滑块会把亮段切开）
	int maxGap = config.slider_detect_merge_gap;
	if (maxGap < 0) maxGap = 0;
	std::vector<BrightRun> merged;
	merged.push_back(runs[0]);
	for (size_t i = 1; i < runs.size(); i++) {
		BrightRun& last = merged.back();
		int lastEnd = last.start + last.len;
		int gap = runs[i].start - lastEnd;
		if (gap <= maxGap) {
			last.len = (runs[i].start + runs[i].len) - last.start;
		} else {
			merged.push_back(runs[i]);
		}
	}

	// 取最长段
	int bestIdx = 0;
	for (size_t i = 1; i < merged.size(); i++) {
		if (merged[i].len > merged[bestIdx].len) bestIdx = (int)i;
	}
	int bestRunStart = merged[bestIdx].start;
	int bestRunLen = merged[bestIdx].len;

	if (bestRunLen < minSliderHeight) return false;
	// 防止误把整条轨道（或背景高亮）当成滑块
	if (bestRunLen >= (int)(roi.height * 0.95)) return false;

	if (sliderTopOut) *sliderTopOut = bestRunStart;
	if (sliderBottomOut) *sliderBottomOut = bestRunStart + bestRunLen;
	if (sliderCenterYOut) *sliderCenterYOut = bestRunStart + bestRunLen / 2;
	return true;
}

static Point2f affineTransformPoint(const Mat& M, const Point2f& p) {
	// M: 2x3
	Point2f out{};
	if (M.empty() || M.rows != 2 || M.cols != 3) {
		return p;
	}
	out.x = (float)(M.at<double>(0, 0) * p.x + M.at<double>(0, 1) * p.y + M.at<double>(0, 2));
	out.y = (float)(M.at<double>(1, 0) * p.x + M.at<double>(1, 1) * p.y + M.at<double>(1, 2));
	return out;
}

bool fillFishSliderResult(const Mat& gray, const Rect& roi, const TplMatch& fish, const TplMatch& slider,
	double trackAngleDeg, int fishTplHeightHint, FishSliderResult* result) {
	if (!result) return false;
	*result = FishSliderResult{};

	result->fishScore = fish.score;
	result->sliderScore = slider.score;
	if (fish.score < config.fish_icon_threshold) {
		return false;
	}

	result->fishX = fish.center.x;
	result->fishY = fish.center.y;
	// 不再依赖滑块模板位置：默认用 fishX 作为滑块X（只用于日志/调试，控制只用 Y）
	result->sliderCenterX = fish.center.x;
	result->sliderCenterY = slider.center.y;

	// 颜色检测：优先在整个轨道 ROI 内扫亮度线条（不依赖滑块模板分数/位置）
	// minSliderHeight 过小会把"鱼图标"这种短亮段误当成滑块，这里做个下限保护
	int effectiveMinH = config.slider_min_height;
	int fishTplH = fishTplHeightHint;
	if (fishTplH <= 0) fishTplH = params.tpl_vr_fish_icon.rows();
	if (fishTplH > 0 && effectiveMinH < fishTplH + 5) effectiveMinH = fishTplH + 5;
	int sliderTop = 0, sliderBottom = 0, sliderCenterFromColor = 0;
	{
		Rect r = clampRect(roi, gray.size());
		Mat roiGray = gray(r);
		Rect localRoi(0, 0, roiGray.cols, roiGray.rows);

		// 旋转矫正：轨道检测得到 angle 后，亮度检测也按同角度把 ROI 旋回到"竖直"再扫（提高鲁棒性）
		double a = trackAngleDeg;
		if (!std::isfinite(a)) a = 0.0;
		Mat scanGray = roiGray; // default: no rotation
		Mat rotated;
		Mat M, Minv;
		int barXLocal = fish.center.x - r.x;
		int barYLocal = fish.center.y - r.y;
		if (barXLocal < 0) barXLocal = 0;
		if (barXLocal >= roiGray.cols) barXLocal = roiGray.cols - 1;
		if (barYLocal < 0) barYLocal = 0;
		if (barYLocal >= roiGray.rows) barYLocal = roiGray.rows - 1;

		float barXForMap = (float)barXLocal; // original-local x for mapping
		if (std::abs(a) > 1e-6 && roiGray.cols > 1 && roiGray.rows > 1) {
			Point2f c((float)roiGray.cols / 2.0f, (float)roiGray.rows / 2.0f);
			M = getRotationMatrix2D(c, -a, 1.0);
			Minv = getRotationMatrix2D(c, +a, 1.0);
			warpAffine(roiGray, rotated, M, roiGray.size(), INTER_LINEAR, BORDER_CONSTANT, Scalar(0));
			scanGray = rotated;

			// barX 也需要变换到旋转后的坐标系下（用于窄条扫描兜底 + 反变换坐标）
			Point2f barRot = affineTransformPoint(M, Point2f((float)barXLocal, (float)barYLocal));
			barXForMap = barRot.x;
			if (barXForMap < 0.0f) barXForMap = 0.0f;
			if (barXForMap > (float)(roiGray.cols - 1)) barXForMap = (float)(roiGray.cols - 1);
			barXLocal = (int)std::round(barXForMap);
		}

		int t = 0, b = 0, cy = 0;
		bool okColor = detectSliderBoundsWide(scanGray, localRoi,
			&t, &b, &cy,
			config.slider_bright_thresh, effectiveMinH)
			|| detectSliderBounds(scanGray, barXLocal, localRoi,
				&t, &b, &cy,
				config.slider_bright_thresh, effectiveMinH);

		if (okColor) {
			// 坐标反变换回原图坐标（保持与 fishY / fixedTrackRoi.y 一致的全图坐标系）
			if (!Minv.empty()) {
				Point2f pTop = affineTransformPoint(Minv, Point2f(barXForMap, (float)t));
				Point2f pBot = affineTransformPoint(Minv, Point2f(barXForMap, (float)b));
				Point2f pCy = affineTransformPoint(Minv, Point2f(barXForMap, (float)cy));
				sliderTop = (int)std::round(pTop.y) + r.y;
				sliderBottom = (int)std::round(pBot.y) + r.y;
				sliderCenterFromColor = (int)std::round(pCy.y) + r.y;
			} else {
				sliderTop = t + r.y;
				sliderBottom = b + r.y;
				sliderCenterFromColor = cy + r.y;
			}

			int maxY = gray.rows > 0 ? (gray.rows - 1) : 0;
			if (sliderTop < 0) sliderTop = 0;
			if (sliderTop > maxY) sliderTop = maxY;
			if (sliderBottom < sliderTop) sliderBottom = sliderTop;
			if (sliderBottom > gray.rows) sliderBottom = gray.rows;
			if (sliderCenterFromColor < 0) sliderCenterFromColor = 0;
			if (sliderCenterFromColor > maxY) sliderCenterFromColor = maxY;

			result->sliderTop = sliderTop;
			result->sliderBottom = sliderBottom;
			result->sliderHeight = sliderBottom - sliderTop;
			result->sliderCenterY = sliderCenterFromColor;  // 用颜色检测的中心覆盖模板匹配的中心
			result->hasBounds = true;
		}
	}

	if (!result->hasBounds) {
		// 颜色检测失败 → 退回滑块模板（作为兜底）
		if (slider.score < config.slider_threshold) {
			return false;
		}
		int tplH = params.tpl_vr_player_slider.rows();
		result->sliderTop = slider.center.y - tplH / 2;
		result->sliderBottom = slider.center.y + tplH / 2;
		result->sliderHeight = tplH;
		result->hasBounds = false;
	}

	return true;
}

// 快速检测（控制循环用）：固定 trackScale/angle + 单鱼模板，仅 2 次 matchTemplate
// cachedFishTplIdx: params.tpl_vr_fish_icons 的索引（fish_icon + fish_icon_alt*.png）
bool detectFishAndSliderFast(const Mat& gray, const Rect& barRect, FishSliderResult* result,
	double trackScale, double trackAngleDeg, int cachedFishTplIdx) {
	Rect roi = clampRect(barRect, gray.size());
	if (roi.width <= 0 || roi.height <= 0) return false;

	double s = trackScale;
	if (!std::isfinite(s) || s <= 0.0) s = 1.0;

	const g_params::GrayTpl* fishTplPtr = &params.tpl_vr_fish_icon;
	if (!params.tpl_vr_fish_icons.empty()) {
		int idx = cachedFishTplIdx;
		if (idx < 0) idx = 0;
		if (idx >= (int)params.tpl_vr_fish_icons.size()) idx = 0;
		fishTplPtr = &params.tpl_vr_fish_icons[(size_t)idx];
	}
	const auto& fishTpl = *fishTplPtr;
	TplMatch fish = matchBestRoiAtScaleAndAngle(gray, fishTpl, roi, s, trackAngleDeg, TM_CCOEFF_NORMED);
	TplMatch slider = matchBestRoi(gray, params.tpl_vr_player_slider, roi, TM_CCORR_NORMED);
	int fishTplHScaled = (int)std::round((double)fishTpl.rows() * s);
	return fillFishSliderResult(gray, roi, fish, slider, trackAngleDeg, fishTplHScaled, result);
}

// 全检测（首帧 + 周期性刷新用）：固定 trackScale/angle 下尝试多鱼模板，输出最佳模板索引
bool detectFishAndSliderFull(const Mat& gray, const Rect& barRect, FishSliderResult* result,
	double trackScale, double trackAngleDeg, int* bestTplIdxOut) {
	Rect roi = clampRect(barRect, gray.size());
	if (roi.width <= 0 || roi.height <= 0) return false;

	double s = trackScale;
	if (!std::isfinite(s) || s <= 0.0) s = 1.0;

	if (params.tpl_vr_fish_icons.empty()) {
		return false;
	}
	TplMatch fish{};
	int bestIdx = 0;
	for (size_t i = 0; i < params.tpl_vr_fish_icons.size(); i++) {
		const auto& tpl = params.tpl_vr_fish_icons[i];
		TplMatch m = matchBestRoiAtScaleAndAngle(gray, tpl, roi, s, trackAngleDeg, TM_CCOEFF_NORMED);
		if (m.score > fish.score) {
			fish = m;
			bestIdx = (int)i;
		}
	}

	TplMatch slider = matchBestRoi(gray, params.tpl_vr_player_slider, roi, TM_CCORR_NORMED);
	int fishTplHScaled = (int)std::round((double)params.tpl_vr_fish_icons[(size_t)bestIdx].rows() * s);
	bool ok = fillFishSliderResult(gray, roi, fish, slider, trackAngleDeg, fishTplHScaled, result);
	if (!ok) {
		return false;
	}
	if (bestTplIdxOut) *bestTplIdxOut = bestIdx;
	return true;
}
