#include "debug_frame.h"

#include <algorithm>
#include <string>

#define NOMINMAX
#include <windows.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "../gui/gui_config_def.h"
#include "../infra/fs/path_utils.h"

extern g_config config;

using namespace cv;

std::string makeDebugPath(const std::string& tag) {
	std::string dir = config.vr_debug_dir.empty() ? "debug_vrchat" : config.vr_debug_dir;
	// Make path absolute if relative (SHCreateDirectoryExA needs absolute paths)
	if (dir.size() < 2 || (dir[1] != ':' && dir[0] != '\\')) {
		char cwd[MAX_PATH];
		if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
			dir = std::string(cwd) + "\\" + dir;
		}
	}
	std::replace(dir.begin(), dir.end(), '/', '\\');
	infra::fs::ensureDirExists(dir);
	return dir + "\\" + tag + "_" + std::to_string(GetTickCount64()) + ".png";
}

void saveDebugFrame(const Mat& bgr, const std::string& tag) {
	if (!config.vr_debug_pic) {
		return;
	}
	if (bgr.empty()) {
		return;
	}
	imwrite(makeDebugPath(tag), bgr);
}

void saveDebugFrame(const Mat& bgr, const std::string& tag, const Rect& r1, const Scalar& c1) {
	if (!config.vr_debug_pic) {
		return;
	}
	if (bgr.empty()) {
		return;
	}
	Mat out = bgr.clone();
	rectangle(out, r1, c1, 2, 8, 0);
	imwrite(makeDebugPath(tag), out);
}

void saveDebugFrame(const Mat& bgr, const std::string& tag, const Rect& r1, const Rect& r2) {
	if (!config.vr_debug_pic) {
		return;
	}
	if (bgr.empty()) {
		return;
	}
	Mat out = bgr.clone();
	rectangle(out, r1, Scalar(0, 0, 255), 2, 8, 0);
	rectangle(out, r2, Scalar(0, 255, 0), 2, 8, 0);
	imwrite(makeDebugPath(tag), out);
}

void saveDebugFrame(const Mat& bgr, const std::string& tag, const Rect& r1, const Rect& r2, const Rect& r3) {
	if (!config.vr_debug_pic) {
		return;
	}
	if (bgr.empty()) {
		return;
	}
	Mat out = bgr.clone();
	rectangle(out, r1, Scalar(255, 0, 0), 2, 8, 0);   // blue: searchRoi
	rectangle(out, r2, Scalar(0, 255, 0), 2, 8, 0);   // green: template match position
	rectangle(out, r3, Scalar(0, 0, 255), 2, 8, 0);   // red: final locked ROI
	imwrite(makeDebugPath(tag), out);
}
