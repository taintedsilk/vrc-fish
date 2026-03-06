#include "matcher.h"
#include "../gui/gui_config_def.h"
#include <cmath>
#include <algorithm>

using namespace cv;

cv::Rect clampRect(cv::Rect r, const cv::Size& bounds) {
	if (r.x < 0) {
		r.width += r.x;
		r.x = 0;
	}
	if (r.y < 0) {
		r.height += r.y;
		r.y = 0;
	}
	if (r.x + r.width > bounds.width) {
		r.width = bounds.width - r.x;
	}
	if (r.y + r.height > bounds.height) {
		r.height = bounds.height - r.y;
	}
	if (r.width < 0) r.width = 0;
	if (r.height < 0) r.height = 0;
	return r;
}

cv::Rect centerThirdStripRoi(const cv::Size& bounds) {
	int w = bounds.width;
	int h = bounds.height;
	int x1 = w / 3;
	int x2 = (w * 2) / 3;
	return clampRect(Rect(x1, 0, x2 - x1, h), bounds);
}

TplMatch matchBest(const Mat& srcGray, const GrayTpl& tpl, int defaultMethod) {
	TplMatch out{};
	const Mat& tplGray = tpl.gray;
	if (srcGray.empty() || tplGray.empty()) {
		return out;
	}
	if (srcGray.cols < tplGray.cols || srcGray.rows < tplGray.rows) {
		return out;
	}
	Mat result;
	int method = defaultMethod;
	if (!tpl.mask.empty()) {
		matchTemplate(srcGray, tplGray, result, method, tpl.mask);
	} else {
		matchTemplate(srcGray, tplGray, result, method);
	}
	double minVal = 0.0, maxVal = 0.0;
	Point minLoc{}, maxLoc{};
	minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
	Point bestLoc = maxLoc;
	double score = maxVal;
	if (method == TM_SQDIFF || method == TM_SQDIFF_NORMED) {
		bestLoc = minLoc;
		score = 1.0 - minVal;
	}
	if (!std::isfinite(score)) {
		score = 0.0;
	}
	out.topLeft = bestLoc;
	out.rect = Rect(bestLoc.x, bestLoc.y, tplGray.cols, tplGray.rows);
	out.center = Point(bestLoc.x + tplGray.cols / 2, bestLoc.y + tplGray.rows / 2);
	out.score = score;
	return out;
}

TplMatch matchBestRoi(const Mat& srcGray, const GrayTpl& tpl, Rect roi, int method) {
	TplMatch out{};
	if (srcGray.empty() || tpl.empty()) {
		return out;
	}
	roi = clampRect(roi, srcGray.size());
	if (roi.width < tpl.cols() || roi.height < tpl.rows()) {
		return out;
	}
	Mat sub = srcGray(roi);
	out = matchBest(sub, tpl, method);
	out.topLeft += roi.tl();
	out.center += roi.tl();
	out.rect.x += roi.x;
	out.rect.y += roi.y;
	return out;
}

static TplMatch matchBestRoiMultiScaleByScales(const Mat& srcGray, const GrayTpl& tpl, Rect roi,
	const double* scales, int scaleCount, int method = TM_CCOEFF_NORMED, double* bestScaleOut = nullptr) {
	TplMatch best{};
	if (srcGray.empty() || tpl.empty()) return best;
	roi = clampRect(roi, srcGray.size());
	if (roi.width <= 0 || roi.height <= 0) return best;
	Mat sub = srcGray(roi);

	double bestScale = 1.0;
	if (!scales || scaleCount <= 0) {
		// fallback: single scale=1.0
		GrayTpl scaled{};
		scaled.gray = tpl.gray;
		scaled.mask = tpl.mask;
		if (sub.cols >= scaled.gray.cols && sub.rows >= scaled.gray.rows) {
			TplMatch m = matchBest(sub, scaled, method);
			best = m;
			bestScale = 1.0;
		}
	} else {
		for (int i = 0; i < scaleCount; i++) {
			double s = scales[i];
			if (!std::isfinite(s) || s <= 0.0) {
				continue;
			}
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
			if (sub.cols < scaled.gray.cols || sub.rows < scaled.gray.rows) continue;
			TplMatch m = matchBest(sub, scaled, method);
			if (m.score > best.score) {
				best = m;
				bestScale = s;
			}
		}
	}
	best.topLeft += roi.tl();
	best.center += roi.tl();
	best.rect.x += roi.x;
	best.rect.y += roi.y;
	if (bestScaleOut) *bestScaleOut = bestScale;
	return best;
}

TplMatch matchBestRoiMultiScale(const Mat& srcGray, const GrayTpl& tpl, Rect roi, int method, double* bestScaleOut) {
	const double fishScales[4] = {
		config.fish_scale_1,
		config.fish_scale_2,
		config.fish_scale_3,
		config.fish_scale_4
	};
	return matchBestRoiMultiScaleByScales(srcGray, tpl, roi, fishScales, 4, method, bestScaleOut);
}

static TplMatch matchBestRoiAtScale(const Mat& srcGray, const GrayTpl& tpl, Rect roi, double scale, int method = TM_CCOEFF_NORMED) {
	TplMatch out{};
	if (srcGray.empty() || tpl.empty()) return out;
	roi = clampRect(roi, srcGray.size());
	if (roi.width <= 0 || roi.height <= 0) return out;
	Mat sub = srcGray(roi);

	GrayTpl scaled{};
	if (std::abs(scale - 1.0) < 1e-6) {
		scaled.gray = tpl.gray;
		scaled.mask = tpl.mask;
	} else {
		int tw = std::max(1, (int)std::round(tpl.gray.cols * scale));
		int th = std::max(1, (int)std::round(tpl.gray.rows * scale));
		resize(tpl.gray, scaled.gray, Size(tw, th), 0, 0, INTER_AREA);
		if (!tpl.mask.empty()) {
			resize(tpl.mask, scaled.mask, Size(tw, th), 0, 0, INTER_NEAREST);
		}
	}
	if (sub.cols < scaled.gray.cols || sub.rows < scaled.gray.rows) return out;
	out = matchBest(sub, scaled, method);
	out.topLeft += roi.tl();
	out.center += roi.tl();
	out.rect.x += roi.x;
	out.rect.y += roi.y;
	return out;
}

static void sortUniqueScales(std::vector<double>& scales) {
	std::sort(scales.begin(), scales.end());
	std::vector<double> uniq;
	uniq.reserve(scales.size());
	for (double s : scales) {
		if (!std::isfinite(s) || s <= 0.0) continue;
		if (uniq.empty() || std::abs(uniq.back() - s) > 1e-6) {
			uniq.push_back(s);
		}
	}
	scales.swap(uniq);
}

static void sortUniqueAngles(std::vector<double>& angles) {
	std::sort(angles.begin(), angles.end());
	std::vector<double> uniq;
	uniq.reserve(angles.size());
	for (double a : angles) {
		if (!std::isfinite(a)) continue;
		if (uniq.empty() || std::abs(uniq.back() - a) > 1e-6) {
			uniq.push_back(a);
		}
	}
	angles.swap(uniq);
}

static std::vector<double> buildScaleRange(double minScale, double maxScale, double step, int maxCount = 128) {
	std::vector<double> scales;
	if (!std::isfinite(minScale) || !std::isfinite(maxScale) || !std::isfinite(step)) return scales;
	if (step <= 0.0) return scales;
	if (minScale > maxScale) std::swap(minScale, maxScale);
	if (minScale <= 0.0 || maxScale <= 0.0) return scales;
	if (maxCount < 2) maxCount = 2;

	double span = maxScale - minScale;
	int count = (int)std::floor(span / step + 1e-9) + 1;
	if (count < 1) count = 1;
	if (count > maxCount) {
		step = (span <= 0.0) ? step : (span / (double)(maxCount - 1));
		count = maxCount;
	}

	scales.reserve(count + 1);
	for (int i = 0; i < count; i++) {
		double s = minScale + step * (double)i;
		if (!std::isfinite(s) || s <= 0.0) continue;
		scales.push_back(s);
	}
	if (!scales.empty()) {
		double last = scales.back();
		if (std::abs(last - maxScale) > step * 0.25) {
			scales.push_back(maxScale);
		}
	}
	sortUniqueScales(scales);
	return scales;
}

static std::vector<double> buildAngleRange(double minAngle, double maxAngle, double step, int maxCount = 256) {
	std::vector<double> angles;
	if (!std::isfinite(minAngle) || !std::isfinite(maxAngle) || !std::isfinite(step)) return angles;
	if (step <= 0.0) return angles;
	if (minAngle > maxAngle) std::swap(minAngle, maxAngle);
	if (maxCount < 2) maxCount = 2;

	double span = maxAngle - minAngle;
	int count = (int)std::floor(span / step + 1e-9) + 1;
	if (count < 1) count = 1;
	if (count > maxCount) {
		step = (span <= 0.0) ? step : (span / (double)(maxCount - 1));
		count = maxCount;
	}

	angles.reserve(count + 1);
	for (int i = 0; i < count; i++) {
		double a = minAngle + step * (double)i;
		if (!std::isfinite(a)) continue;
		angles.push_back(a);
	}
	if (!angles.empty()) {
		double last = angles.back();
		if (std::abs(last - maxAngle) > step * 0.25) {
			angles.push_back(maxAngle);
		}
	}
	sortUniqueAngles(angles);
	return angles;
}

static GrayTpl makeScaledTpl(const GrayTpl& tpl, double scale) {
	GrayTpl scaled{};
	if (tpl.empty()) return scaled;

	if (!std::isfinite(scale) || scale <= 0.0 || std::abs(scale - 1.0) < 1e-6) {
		scaled.gray = tpl.gray;
		scaled.mask = tpl.mask;
		return scaled;
	}

	int tw = std::max(1, (int)std::round(tpl.gray.cols * scale));
	int th = std::max(1, (int)std::round(tpl.gray.rows * scale));
	resize(tpl.gray, scaled.gray, Size(tw, th), 0, 0, INTER_AREA);
	if (!tpl.mask.empty()) {
		resize(tpl.mask, scaled.mask, Size(tw, th), 0, 0, INTER_NEAREST);
	}
	return scaled;
}

static GrayTpl rotateTplKeepAll(const GrayTpl& tpl, double angleDeg) {
	GrayTpl out{};
	if (tpl.empty()) return out;
	if (!std::isfinite(angleDeg) || std::abs(angleDeg) < 1e-6) {
		out.gray = tpl.gray;
		out.mask = tpl.mask;
		return out;
	}

	int w = tpl.gray.cols;
	int h = tpl.gray.rows;
	Point2f center((float)w / 2.0f, (float)h / 2.0f);

	RotatedRect rr(center, Size2f((float)w, (float)h), (float)angleDeg);
	Rect2f bbox = rr.boundingRect2f();
	int outW = std::max(1, (int)std::ceil(bbox.width));
	int outH = std::max(1, (int)std::ceil(bbox.height));

	Mat M = getRotationMatrix2D(center, angleDeg, 1.0);
	M.at<double>(0, 2) += (double)outW / 2.0 - center.x;
	M.at<double>(1, 2) += (double)outH / 2.0 - center.y;

	warpAffine(tpl.gray, out.gray, M, Size(outW, outH), INTER_LINEAR, BORDER_CONSTANT, Scalar(0));
	if (!tpl.mask.empty()) {
		warpAffine(tpl.mask, out.mask, M, Size(outW, outH), INTER_NEAREST, BORDER_CONSTANT, Scalar(0));
	}
	return out;
}

TplMatch matchBestRoiAtScaleAndAngle(
	const Mat& srcGray,
	const GrayTpl& tpl,
	Rect roi,
	double scale,
	double angleDeg,
	int method
) {
	if (!std::isfinite(scale) || scale <= 0.0) scale = 1.0;
	if (!std::isfinite(angleDeg)) angleDeg = 0.0;
	GrayTpl scaled = makeScaledTpl(tpl, scale);
	GrayTpl rotated = rotateTplKeepAll(scaled, angleDeg);
	return matchBestRoi(srcGray, rotated, roi, method);
}

struct ScaleMatch {
	double scale = 1.0;
	TplMatch match{};
};

static TplMatch matchBestRoiMultiScaleCoarseToFine(
	const Mat& srcGray,
	const GrayTpl& tpl,
	Rect roi,
	const std::vector<double>& coarseScales,
	int refineTopK,
	double refineRadius,
	double refineStep,
	int method = TM_CCOEFF_NORMED,
	double* bestScaleOut = nullptr
) {
	TplMatch best{};
	if (srcGray.empty() || tpl.empty()) return best;

	double bestScale = 1.0;
	std::vector<ScaleMatch> coarseMatches;
	coarseMatches.reserve(coarseScales.size());

	double coarseMin = 0.0, coarseMax = 0.0;
	bool hasCoarseMinMax = false;

	for (double s : coarseScales) {
		if (!std::isfinite(s) || s <= 0.0) continue;

		if (!hasCoarseMinMax) {
			coarseMin = coarseMax = s;
			hasCoarseMinMax = true;
		} else {
			if (s < coarseMin) coarseMin = s;
			if (s > coarseMax) coarseMax = s;
		}

		TplMatch m = matchBestRoiAtScale(srcGray, tpl, roi, s, method);
		coarseMatches.push_back(ScaleMatch{ s, m });
		if (m.score > best.score) {
			best = m;
			bestScale = s;
		}
	}

	if (coarseMatches.empty()) {
		best = matchBestRoiAtScale(srcGray, tpl, roi, 1.0, method);
		bestScale = 1.0;
		if (bestScaleOut) *bestScaleOut = bestScale;
		return best;
	}

	if (refineTopK < 1) refineTopK = 0;
	if (!std::isfinite(refineRadius) || refineRadius <= 0.0) refineTopK = 0;
	if (!std::isfinite(refineStep) || refineStep <= 0.0) refineTopK = 0;
	if (!hasCoarseMinMax || coarseMax <= 0.0 || coarseMin <= 0.0) refineTopK = 0;

	if (refineTopK > 0) {
		std::sort(coarseMatches.begin(), coarseMatches.end(), [](const ScaleMatch& a, const ScaleMatch& b) {
			return a.match.score > b.match.score;
		});

		std::vector<double> refineScales;
		refineScales.reserve((size_t)refineTopK * 16);

		for (int i = 0; i < (int)coarseMatches.size() && i < refineTopK; i++) {
			const ScaleMatch& cand = coarseMatches[(size_t)i];
			if (cand.match.score <= 0.0) break;

			double s1 = cand.scale - refineRadius;
			double s2 = cand.scale + refineRadius;
			if (s1 < coarseMin) s1 = coarseMin;
			if (s2 > coarseMax) s2 = coarseMax;
			if (s2 <= s1) continue;

			std::vector<double> seg = buildScaleRange(s1, s2, refineStep, 96);
			refineScales.insert(refineScales.end(), seg.begin(), seg.end());
		}

		sortUniqueScales(refineScales);

		for (double s : refineScales) {
			TplMatch m = matchBestRoiAtScale(srcGray, tpl, roi, s, method);
			if (m.score > best.score) {
				best = m;
				bestScale = s;
			}
		}
	}

	if (bestScaleOut) *bestScaleOut = bestScale;
	return best;
}

struct AngleMatch {
	double angleDeg = 0.0;
	TplMatch match{};
};

static TplMatch matchBestRoiMultiAngleAtScaleCoarseToFine(
	const Mat& srcGray,
	const GrayTpl& tpl,
	Rect roi,
	double scale,
	const std::vector<double>& coarseAngles,
	int refineTopK,
	double refineRadius,
	double refineStep,
	int method = TM_CCOEFF_NORMED,
	double* bestAngleOut = nullptr
) {
	TplMatch best{};
	if (srcGray.empty() || tpl.empty()) return best;

	GrayTpl scaledTpl = makeScaledTpl(tpl, scale);
	if (scaledTpl.empty()) return best;

	double bestAngle = 0.0;
	std::vector<AngleMatch> coarseMatches;
	coarseMatches.reserve(coarseAngles.size());

	double coarseMin = 0.0, coarseMax = 0.0;
	bool hasCoarseMinMax = false;

	for (double a : coarseAngles) {
		if (!std::isfinite(a)) continue;
		if (!hasCoarseMinMax) {
			coarseMin = coarseMax = a;
			hasCoarseMinMax = true;
		} else {
			if (a < coarseMin) coarseMin = a;
			if (a > coarseMax) coarseMax = a;
		}

		GrayTpl rotated = rotateTplKeepAll(scaledTpl, a);
		TplMatch m = matchBestRoi(srcGray, rotated, roi, method);
		coarseMatches.push_back(AngleMatch{ a, m });
		if (m.score > best.score) {
			best = m;
			bestAngle = a;
		}
	}

	if (coarseMatches.empty()) {
		// fallback: angle=0
		GrayTpl rotated = rotateTplKeepAll(scaledTpl, 0.0);
		best = matchBestRoi(srcGray, rotated, roi, method);
		bestAngle = 0.0;
		if (bestAngleOut) *bestAngleOut = bestAngle;
		return best;
	}

	if (refineTopK < 1) refineTopK = 0;
	if (!std::isfinite(refineRadius) || refineRadius <= 0.0) refineTopK = 0;
	if (!std::isfinite(refineStep) || refineStep <= 0.0) refineTopK = 0;
	if (!hasCoarseMinMax) refineTopK = 0;

	if (refineTopK > 0) {
		std::sort(coarseMatches.begin(), coarseMatches.end(), [](const AngleMatch& a, const AngleMatch& b) {
			return a.match.score > b.match.score;
		});

		std::vector<double> refineAngles;
		refineAngles.reserve((size_t)refineTopK * 16);

		for (int i = 0; i < (int)coarseMatches.size() && i < refineTopK; i++) {
			const AngleMatch& cand = coarseMatches[(size_t)i];
			if (cand.match.score <= 0.0) break;

			double a1 = cand.angleDeg - refineRadius;
			double a2 = cand.angleDeg + refineRadius;
			if (a1 < coarseMin) a1 = coarseMin;
			if (a2 > coarseMax) a2 = coarseMax;
			if (a2 <= a1) continue;

			std::vector<double> seg = buildAngleRange(a1, a2, refineStep, 128);
			refineAngles.insert(refineAngles.end(), seg.begin(), seg.end());
		}

		sortUniqueAngles(refineAngles);

		for (double a : refineAngles) {
			GrayTpl rotated = rotateTplKeepAll(scaledTpl, a);
			TplMatch m = matchBestRoi(srcGray, rotated, roi, method);
			if (m.score > best.score) {
				best = m;
				bestAngle = a;
			}
		}
	}

	if (bestAngleOut) *bestAngleOut = bestAngle;
	return best;
}

static std::vector<double> buildTrackBarScales() {
	std::vector<double> scales;
	if (std::isfinite(config.track_scale_min) && std::isfinite(config.track_scale_max)
		&& std::isfinite(config.track_scale_step)
		&& config.track_scale_min > 0.0 && config.track_scale_max > 0.0 && config.track_scale_step > 0.0
		&& config.track_scale_max >= config.track_scale_min) {
		scales = buildScaleRange(config.track_scale_min, config.track_scale_max, config.track_scale_step, 128);
	}
	if (scales.empty()) {
		scales = {
			config.track_scale_1,
			config.track_scale_2,
			config.track_scale_3,
			config.track_scale_4
		};
		sortUniqueScales(scales);
	}
	return scales;
}

TplMatch matchBestRoiTrackBarAutoScale(
	const Mat& srcGray,
	const GrayTpl& tpl,
	Rect roi,
	int method,
	double* bestScaleOut,
	double* bestAngleOut
) {
	std::vector<double> coarse = buildTrackBarScales();
	double bestScale = 1.0;
	TplMatch best = matchBestRoiMultiScaleCoarseToFine(
		srcGray,
		tpl,
		roi,
		coarse,
		config.track_scale_refine_topk,
		config.track_scale_refine_radius,
		config.track_scale_refine_step,
		method,
		&bestScale
	);
	double bestAngle = 0.0;
	if (std::isfinite(config.track_angle_min) && std::isfinite(config.track_angle_max)
		&& std::isfinite(config.track_angle_step)
		&& config.track_angle_step > 0.0) {
		double aMin = config.track_angle_min;
		double aMax = config.track_angle_max;
		if (aMin > aMax) std::swap(aMin, aMax);
		std::vector<double> angles = buildAngleRange(aMin, aMax, config.track_angle_step, 256);
		if (!angles.empty()) {
			best = matchBestRoiMultiAngleAtScaleCoarseToFine(
				srcGray,
				tpl,
				roi,
				bestScale,
				angles,
				config.track_angle_refine_topk,
				config.track_angle_refine_radius,
				config.track_angle_refine_step,
				method,
				&bestAngle
			);
		}
	}
	if (bestScaleOut) *bestScaleOut = bestScale;
	if (bestAngleOut) *bestAngleOut = bestAngle;
	return best;
}
