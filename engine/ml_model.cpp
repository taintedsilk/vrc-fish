#include "ml_model.h"

#include <fstream>
#include <sstream>
#include <string>

#include "../gui/gui_logger.h"

MlpModel g_mlpModel;

bool loadMlpWeights(const std::string& path, MlpModel& model) {
	std::ifstream f(path);
	if (!f.is_open()) {
		LOG_ERROR("[ML] Cannot open weights file: %s", path.c_str());
		return false;
	}
	model.layers.clear();
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream hdr(line);
		int in_dim, out_dim;
		if (!(hdr >> in_dim >> out_dim)) continue;
		MlpLayer layer;
		layer.in_dim = in_dim;
		layer.out_dim = out_dim;
		layer.weights.resize(out_dim * in_dim);
		layer.bias.resize(out_dim);
		for (int r = 0; r < out_dim; r++) {
			if (!std::getline(f, line)) { LOG_ERROR("[ML] Weight file format error (weights)"); return false; }
			std::istringstream row(line);
			for (int c = 0; c < in_dim; c++) {
				if (!(row >> layer.weights[r * in_dim + c])) {
					LOG_ERROR("[ML] Weight file format error (weight value)");
					return false;
				}
			}
		}
		if (!std::getline(f, line)) { LOG_ERROR("[ML] Weight file format error (bias)"); return false; }
		std::istringstream brow(line);
		for (int r = 0; r < out_dim; r++) {
			if (!(brow >> layer.bias[r])) {
				LOG_ERROR("[ML] Weight file format error (bias value)");
				return false;
			}
		}
		model.layers.push_back(std::move(layer));
	}
	if (model.layers.empty()) {
		LOG_ERROR("[ML] Weight file is empty");
		return false;
	}
	model.loaded = true;
	LOG_INFO("[ML] Model loaded: %zu layers", model.layers.size());

	// Load normalization params
	std::string normPath = path;
	size_t dotPos = normPath.rfind('.');
	if (dotPos != std::string::npos)
		normPath = normPath.substr(0, dotPos) + "_norm.txt";
	else
		normPath += "_norm";
	std::ifstream nf(normPath);
	if (nf.is_open()) {
		std::string nline;
		model.norm_mean.clear();
		model.norm_std.clear();
		while (std::getline(nf, nline)) {
			if (nline.empty() || nline[0] == '#') continue;
			std::istringstream ns(nline);
			double m, s;
			if (ns >> m >> s) {
				model.norm_mean.push_back(m);
				model.norm_std.push_back(s);
			}
		}
		LOG_INFO("[ML] Normalization params loaded: %zu features", model.norm_mean.size());
	} else {
		LOG_WARN("[ML] Normalization file not found: %s, using raw input", normPath.c_str());
	}
	return true;
}
